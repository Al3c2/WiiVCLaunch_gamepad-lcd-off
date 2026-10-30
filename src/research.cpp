#include "research.h"
#include "logger.h"

#include <coreinit/time.h>
#include <notifications/notifications.h>
#include <nsysccr/cdc.h>
#include <vpadbase/base.h>
#include <vpad/input.h>

#include <cstdio>
#include <cstring>

int32_t gResearchCtrlModePoint = RESEARCH_CTRL_MODE_OFF;
bool gResearchLog              = false;

#define RESEARCH_LOG_PATH "fs:/vol/external01/wiiu/lcdoff-research.log"

static void logLine(const char *line)
{
    if (!gResearchLog)
        return;
    FILE *f = fopen(RESEARCH_LOG_PATH, "a");
    if (!f)
        return;
    OSCalendarTime t;
    OSTicksToCalendarTime(OSGetTime(), &t);
    fprintf(f, "%02d:%02d:%02d.%03d %s\n", t.tm_hour, t.tm_min, t.tm_sec, t.tm_msec, line);
    fclose(f);
}

static void hexDump(char *out, size_t outSize, const uint8_t *data, uint32_t size)
{
    size_t pos = 0;
    out[0]     = '\0';
    for (uint32_t i = 0; i < size && pos + 4 < outSize; i++)
        pos += snprintf(out + pos, outSize - pos, "%02X ", data[i]);
}

// Generic command (libdrc "cmd" protocol, query type 0) to GamePad 0.
// Only used with read-only methods here. The meaning of CCRCDCCommand's first
// arguments is undocumented; 0 is used and the result code is logged.
static uint8_t sReply[0x40] __attribute__((aligned(0x40)));

static int32_t genericRead(uint8_t service, uint8_t method, uint32_t replySize)
{
    memset(sReply, 0, sizeof(sReply));
    return CCRCDCCommand(0, 0, CCR_CDC_DESTINATION_DRC0, 0, 0, service, method, nullptr, 0, sReply, replySize);
}

void researchSnapshot(const char *tag)
{
    char line[256];
    char hex[64];

    int32_t ctrlMode = -1;
    VPADBASEGetGameControllerMode(VPAD_CHAN_0, &ctrlMode);

    CCRCDCDrcState drcState = {0xFF};
    int32_t stateRes        = CCRCDCSysGetDrcState(CCR_CDC_DESTINATION_DRC0, &drcState);

    // service 5 method 0x15: get LCD brightness (1 byte, 1..5) -- known answer, used to check the framing
    int32_t brightRes = genericRead(5, 0x15, 1);
    uint8_t bright    = sReply[0];

    // service 5 method 0x1A: get LCD parameters (13 bytes, read from the LCD controller)
    int32_t lcdRes = genericRead(5, 0x1A, 13);
    hexDump(hex, sizeof(hex), sReply, 13);

    snprintf(line, sizeof(line), "[%s] ctrlMode=%ld drcState=%u (res %ld) brightness=%u (res %ld) lcdParams(res %ld)= %s",
             tag, (long) ctrlMode, drcState.state, (long) stateRes, bright, (long) brightRes, (long) lcdRes, hex);
    logLine(line);
    DEBUG_FUNCTION_LINE("%s", line);

    char note[120];
    snprintf(note, sizeof(note), "[%s] mode %ld state %u bright %u/%ld lcd %ld", tag, (long) ctrlMode, drcState.state, bright, (long) brightRes, (long) lcdRes);
    NotificationModule_AddInfoNotification(note);
}

void researchSetCtrlModeNow(int32_t mode)
{
    researchSnapshot(mode ? "before-off" : "before-on");
    VPADBASESetGameControllerMode(VPAD_CHAN_0, mode);
    if (mode == 0) // controller mode 0 alone doesn't bring the display back; switch the LCD on explicitly
        VPADSetLcdMode(VPAD_CHAN_0, VPAD_LCD_ON);
    researchSnapshot(mode ? "after-off" : "after-on");
}

void researchAtPoint(ResearchPoint point)
{
    static const char *names[] = {"choice", "prepare", "launch"};
    const char *name           = names[point];

    bool apply = gResearchCtrlModePoint == RESEARCH_CTRL_MODE_ALL ||
                 (point == RESEARCH_POINT_CHOICE && gResearchCtrlModePoint == RESEARCH_CTRL_MODE_AT_CHOICE) ||
                 (point == RESEARCH_POINT_PREPARE && gResearchCtrlModePoint == RESEARCH_CTRL_MODE_AT_PREPARE) ||
                 (point == RESEARCH_POINT_LAUNCH && gResearchCtrlModePoint == RESEARCH_CTRL_MODE_AT_LAUNCH);

    if (apply) {
        VPADBASESetGameControllerMode(VPAD_CHAN_0, 1);
        char line[64];
        snprintf(line, sizeof(line), "[%s] VPADBASESetGameControllerMode(0, 1) called", name);
        logLine(line);
    }
    if (gResearchLog)
        researchSnapshot(name);
}

// ---------------------------------------------------------------------------
// Read-only IOSU dumper.
// Reads IOSU module memory with Mocha's kernel read (the same call evWii uses)
// and writes it to SD:/wiiu/iosu-dump/. Nothing in IOSU is written.
// Module ranges from wiiubrew CAFE-IOS (5.5.0+), virtual addresses.
// ---------------------------------------------------------------------------
#include "mocha.h"
#include <coreinit/mcp.h>
#include <coreinit/thread.h>
#include <malloc.h>
#include <sys/stat.h>
#include <thread>

struct IosuRegion {
    const char *name;
    uint32_t start;
    uint32_t size;
};

// Third pass: IOS-PAD only (GamePad driver). Pass 2 got MCP rodata/data; reading BSP at 0xE6042000 stopped the console.
static const IosuRegion sIosuRegions[] = {
        {"IOS-PAD", 0x11F00000, 0x00260000},           // GamePad: /dev/ccr_io, ccr_cdc, ccr_hid, DRH
        {"IOS-MCP-text-tail", 0x05058000, 0x00008000}, // where pass 1 stopped -- last on purpose
};

#define IOSU_DUMP_DIR "fs:/vol/external01/wiiu/iosu-dump"
static bool sIosuDumpRunning = false;

static void iosuDumpNotify(const char *text, bool ok)
{
    if (ok)
        NotificationModule_AddInfoNotification(text);
    else
        NotificationModule_AddErrorNotification(text);
}

static void iosuDumpThread()
{
    char text[160];
    if (initMocha() != MOCHA_RESULT_SUCCESS) {
        iosuDumpNotify("IOSU dump: Mocha not available", false);
        sIosuDumpRunning = false;
        return;
    }

    mkdir("fs:/vol/external01/wiiu", 0777);
    mkdir(IOSU_DUMP_DIR, 0777);

    FILE *info = fopen(IOSU_DUMP_DIR "/info.txt", "a");
    if (info) {
        int32_t mcp                       = MCP_Open();
        MCPSystemVersion ver __attribute__((aligned(0x40))) = {};
        if (mcp >= 0 && MCP_GetSystemVersion(mcp, &ver) == 0)
            fprintf(info, "System version: %lu.%lu.%lu%c\n", (unsigned long) ver.major, (unsigned long) ver.minor, (unsigned long) ver.patch, ver.region);
        if (mcp >= 0)
            MCP_Close(mcp);
        fprintf(info, "Read with Mocha_IOSUKernelRead32, virtual addresses.\nUnreadable words are filled with 0xDEADBEEF and counted below.\n");
        fflush(info);
    }

    const uint32_t chunkSize = 0x1000;
    uint32_t *chunk          = (uint32_t *) memalign(0x40, chunkSize);
    bool allOk               = chunk != nullptr;

    for (const auto &r : sIosuRegions) {
        if (!chunk)
            break;
        snprintf(text, sizeof(text), "IOSU dump: %s (%lu KB)...", r.name, (unsigned long) (r.size / 1024));
        iosuDumpNotify(text, true);

        char path[96];
        snprintf(path, sizeof(path), IOSU_DUMP_DIR "/%s_%08lX.bin", r.name, (unsigned long) r.start);
        FILE *f = fopen(path, "wb");
        if (!f) {
            snprintf(text, sizeof(text), "IOSU dump: can't create %s", path);
            iosuDumpNotify(text, false);
            allOk = false;
            continue;
        }

        uint32_t failed = 0;
        for (uint32_t off = 0; off < r.size; off += chunkSize) {
            for (uint32_t i = 0; i < chunkSize / 4; i++) {
                uint32_t v = 0xDEADBEEF;
                if (Mocha_IOSUKernelRead32(r.start + off + i * 4, &v) != MOCHA_RESULT_SUCCESS) {
                    v = 0xDEADBEEF;
                    failed++;
                }
                chunk[i] = v; // PowerPC is big-endian, like the ARM IOSU here: bytes are written as-is
            }
            fwrite(chunk, 1, chunkSize, f);
            if (((off + chunkSize) & 0xFFFF) == 0 || off + chunkSize >= r.size) {
                fflush(f); // keep what we have if the console stops
                if (info) {
                    fprintf(info, "progress %s up to 0x%08lX\n", r.name, (unsigned long) (r.start + off + chunkSize));
                    fflush(info);
                }
            }
        }
        fclose(f);
        if (info)
        {
            fprintf(info, "%s 0x%08lX size 0x%08lX failed words %lu\n", r.name, (unsigned long) r.start, (unsigned long) r.size, (unsigned long) failed);
            fflush(info);
        }
        if (failed)
            allOk = false;
    }

    if (info)
        fclose(info);
    free(chunk);
    Mocha_DeInitLibrary();

    iosuDumpNotify(allOk ? "IOSU dump: done, files in SD:/wiiu/iosu-dump" : "IOSU dump: finished with errors, see SD:/wiiu/iosu-dump/info.txt", allOk);
    sIosuDumpRunning = false;
}

void researchStartIosuDump()
{
    if (sIosuDumpRunning) {
        iosuDumpNotify("IOSU dump: already running", false);
        return;
    }
    sIosuDumpRunning = true;
    std::thread(iosuDumpThread).detach();
}
