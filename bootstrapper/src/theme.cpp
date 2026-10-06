#include "theme.h"

const Theme& DarkTheme() {
    static const Theme theme = [] {
        Theme t{};
        t.dark = true;
        t.windowBg = ThemeColor(0.10f, 0.10f, 0.13f);
        t.panelBg = ThemeColor(0.16f, 0.16f, 0.21f);
        t.button = ThemeColor(0.13f, 0.13f, 0.17f);
        t.buttonHover = ThemeColor(0.19f, 0.20f, 0.25f);
        t.buttonActive = ThemeColor(0.16f, 0.16f, 0.21f);
        t.text = ThemeColor(1.00f, 1.00f, 1.00f);
        t.muted = ThemeColor(0.50f, 0.50f, 0.50f);
        t.accent = ThemeColor(0.74f, 0.58f, 0.98f);
        t.onAccent = ThemeColor(0.10f, 0.10f, 0.13f);
        t.border = ThemeColor(0.44f, 0.37f, 0.61f);
        t.error = ThemeColor(0.95f, 0.45f, 0.45f);
        return t;
    }();
    return theme;
}

const Theme& LightTheme() {
    static const Theme theme = [] {
        Theme t{};
        t.dark = false;
        t.windowBg = ThemeColor(0.97f, 0.97f, 0.98f);
        t.panelBg = ThemeColor(0.90f, 0.90f, 0.93f);
        t.button = ThemeColor(0.88f, 0.88f, 0.91f);
        t.buttonHover = ThemeColor(0.82f, 0.82f, 0.87f);
        t.buttonActive = ThemeColor(0.78f, 0.78f, 0.84f);
        t.text = ThemeColor(0.10f, 0.10f, 0.13f);
        t.muted = ThemeColor(0.42f, 0.42f, 0.46f);
        t.accent = ThemeColor(0.55f, 0.40f, 0.85f);
        t.onAccent = ThemeColor(1.00f, 1.00f, 1.00f);
        t.border = ThemeColor(0.70f, 0.66f, 0.80f);
        t.error = ThemeColor(0.80f, 0.20f, 0.20f);
        return t;
    }();
    return theme;
}

bool SystemUsesLightTheme() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &key) != ERROR_SUCCESS) return false;

    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    const bool ok = RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS;
    RegCloseKey(key);

    return ok && type == REG_DWORD && value != 0;
}
