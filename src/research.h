#pragma once

#include <cstdint>

// LCD-off research experiments (branch research/lcd-off).
// Everything here is either read-only or uses documented display-off calls.

enum ResearchCtrlModePoint {
    RESEARCH_CTRL_MODE_OFF,      // don't call VPADBASESetGameControllerMode
    RESEARCH_CTRL_MODE_AT_CHOICE, // Wii U Menu, when "Use GamePad as controller" is chosen
    RESEARCH_CTRL_MODE_AT_PREPARE, // CMPTExPrepareLaunch (game process)
    RESEARCH_CTRL_MODE_AT_LAUNCH, // right before MCP_LaunchCompat
    RESEARCH_CTRL_MODE_ALL,       // all three
    RESEARCH_CTRL_MODE_COUNT,
};

enum ResearchPoint {
    RESEARCH_POINT_CHOICE,
    RESEARCH_POINT_PREPARE,
    RESEARCH_POINT_LAUNCH,
};

extern int32_t gResearchCtrlModePoint;
extern bool gResearchLog;

// Route 1: called from the three launch hooks
void researchAtPoint(ResearchPoint point);

// Route 1 live test from the plugin menu: mode != 0 turns the display off now
void researchSetCtrlModeNow(int32_t mode);

// Read-only snapshot (controller mode, GamePad state, brightness and LCD parameters via generic commands)
// appended to SD:/wiiu/lcdoff-research.log, with a short notification
void researchSnapshot(const char *tag);

// Read-only IOSU dump to SD:/wiiu/iosu-dump/ (IOS-MCP, IOS-PAD, IOS-BSP), runs in a background thread
void researchStartIosuDump();
