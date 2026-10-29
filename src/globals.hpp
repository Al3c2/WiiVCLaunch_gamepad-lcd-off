#pragma once

#include <cstdint>

//values must align with corresponding values in enum CmptScreenType in <nn/cmpt/cmpt.h> in wut
enum DisplayOption {
    DISPLAY_OPTION_USE_DRC,
    DISPLAY_OPTION_TV,
    DISPLAY_OPTION_DRC,
    DISPLAY_OPTION_BOTH,
    DISPLAY_OPTION_CHOOSE,
    DISPLAY_OPTION_COUNT,
};

enum DisplayOptionsOrder {
    DISPLAY_OPTIONS_ORDER_DEFAULT,
    DISPLAY_OPTIONS_ORDER_RECENT,
    DISPLAY_OPTIONS_ORDER_COUNT,
};

//values must align with corresponding values in enum AVMTvResolution in <avm/tv.h> in wut
enum SetResolution {
    SET_RESOLUTION_NONE    = 0,
    SET_RESOLUTION_576I    = 1,
    SET_RESOLUTION_480I    = 2,
    SET_RESOLUTION_480P    = 3,
    SET_RESOLUTION_720P    = 4,
    SET_RESOLUTION_1080I   = 6,
    SET_RESOLUTION_1080P   = 7,
    SET_RESOLUTION_576I_43 = 101,
    SET_RESOLUTION_480I_43 = 102,
    SET_RESOLUTION_480P_43 = 103,
};

// GamePad screen off (LCD-off fork)
enum DrcLcdOffMode {
    DRC_LCD_OFF_DISABLED,       // stock behaviour, screen stays on
    DRC_LCD_OFF_AT_LAUNCH,      // VPADSetLcdMode(OFF) right before MCP_LaunchCompat
    DRC_LCD_OFF_AT_LAUNCH_100,  // same, then wait 100 ms
    DRC_LCD_OFF_AT_LAUNCH_500,  // same, then wait 500 ms
    DRC_LCD_OFF_AT_PREPARE,     // earlier: in CMPTExPrepareLaunch
    DRC_LCD_OFF_COUNT,
};

// GamePad picture when it is used as controller (TV-only fork)
enum DrcPictureMode {
    DRC_PICTURE_MIRROR,          // stock: GamePad mirrors the TV
    DRC_PICTURE_TV_ONLY_BEFORE,  // screen type TV only, set before enabling GamePad control
    DRC_PICTURE_TV_ONLY_AFTER,   // screen type TV only, set after enabling GamePad control
    DRC_PICTURE_COUNT,
};

// GamePad brightness at launch (brightness fork)
enum DrcBrightnessMode {
    DRC_BRIGHTNESS_UNCHANGED,  // stock
    DRC_BRIGHTNESS_ZERO,       // experimental: level 0 (below the official 1..5) = backlight off?
    DRC_BRIGHTNESS_ONE,        // lowest official level
    DRC_BRIGHTNESS_COUNT,
};

enum NotificationTheme {
    NOTIFICATION_THEME_OFF,
    NOTIFICATION_THEME_DARK,
    NOTIFICATION_THEME_LIGHT,
    NOTIFICATION_THEME_COUNT,
};

#define SET_RESOLUTION_4_3_MODIFIER 100

// Wii VC
extern int32_t gAutolaunchDRCSupported;
extern int32_t gAutolaunchNoDRCSupport;
extern int32_t gDisplayOptionsOrder;
extern int32_t gSetResolution;
extern bool gUseCustomDialogs;

// Wii Mode
extern int32_t gWiiMenuSetResolution;

// WUHB Forwarder
extern int32_t gForwarderDisplayOverride;

// GamePad screen off
extern int32_t gDrcLcdOffMode;
extern bool gDrcUsedAsController;
extern int32_t gDrcPictureMode;
extern int32_t gDrcBrightnessMode;

// restores the GamePad brightness saved before a Wii VC launch (safe to call any time)
void restoreDrcBrightnessIfNeeded();

// Other
extern int32_t gNotificationTheme;
extern bool gPreserveSysconf;
extern bool gPermanentNetConfig;

extern bool gInWiiUMenu;
