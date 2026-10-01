#pragma once
#include "pch.h"
#include <atomic>

class Menu {
public:
    static inline std::atomic<bool> initialized{false};
    static inline std::atomic<bool> visible{false};

    static void Initialize();
    static void Shutdown();
private:
    static void InitStyle();

    static void RenderMainWindow();
    static void RenderSettingsTab();

    static void Render();
    static LRESULT HandleInput(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};
