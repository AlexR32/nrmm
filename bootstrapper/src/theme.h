#pragma once

#include "framework.h"

struct Theme {
    bool dark;
    COLORREF windowBg;
    COLORREF panelBg;
    COLORREF button;
    COLORREF buttonHover;
    COLORREF buttonActive;
    COLORREF text;
    COLORREF muted;
    COLORREF accent;
    COLORREF onAccent;
    COLORREF border;
    COLORREF error;
};

inline COLORREF ThemeColor(float r, float g, float b) {
    return RGB(static_cast<int>(r * 255.0f + 0.5f), static_cast<int>(g * 255.0f + 0.5f), static_cast<int>(b * 255.0f + 0.5f));
}

const Theme& DarkTheme();
const Theme& LightTheme();

// Reads HKCU AppsUseLightTheme. Returns true when Windows is in light mode.
bool SystemUsesLightTheme();
