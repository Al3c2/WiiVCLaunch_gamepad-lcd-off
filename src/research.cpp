#include "research.h"
#include "logger.h"

#include <coreinit/time.h>
#include <notifications/notifications.h>
#include <nsysccr/cdc.h>
#include <vpadbase/base.h>

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
