#pragma once
#include "pch.h"
#include "menu.h"
#include "libs/minhook/include/MinHook.h"

// Unity pins the cursor to the middle of the window and clips it while
// Cursor.lockState is Locked/Confined, so the ImGui cursor rubber-bands back

// While the menu is visible the engine's re-centering calls are swallowed and
// the clip region is released; closing the menu restores both

class Cursor {
public:
    static void Install() {
        if (installed) return;

        HMODULE user32 = GetModuleHandleA("user32.dll");
        if (!user32) return;

        void* setCursorPos = reinterpret_cast<void*>(GetProcAddress(user32, "SetCursorPos"));
        void* clipCursor = reinterpret_cast<void*>(GetProcAddress(user32, "ClipCursor"));

        if (setCursorPos && MH_CreateHook(setCursorPos, reinterpret_cast<void*>(&hkSetCursorPos), reinterpret_cast<void**>(&oSetCursorPos)) == MH_OK)
            MH_EnableHook(setCursorPos);

        if (clipCursor && MH_CreateHook(clipCursor, reinterpret_cast<void*>(&hkClipCursor), reinterpret_cast<void**>(&oClipCursor)) == MH_OK)
            MH_EnableHook(clipCursor);

        installed = true;
    }

    static void Uninstall() {
        if (!installed) return;

        HMODULE user32 = GetModuleHandleA("user32.dll");
        if (user32) {
            void* setCursorPos = reinterpret_cast<void*>(GetProcAddress(user32, "SetCursorPos"));
            void* clipCursor = reinterpret_cast<void*>(GetProcAddress(user32, "ClipCursor"));

            if (setCursorPos && oSetCursorPos) {
                MH_DisableHook(setCursorPos);
                MH_RemoveHook(setCursorPos);
            }
            if (clipCursor && oClipCursor) {
                MH_DisableHook(clipCursor);
                MH_RemoveHook(clipCursor);
            }
        }

        oSetCursorPos = nullptr;
        oClipCursor = nullptr;
        installed = false;
    }

    // Release any clip region the engine has set (called when the menu opens)
    static void ReleaseClip() {
        if (oClipCursor) oClipCursor(nullptr);
        else ClipCursor(nullptr);
    }

private:
    using SetCursorPosFn = BOOL(WINAPI*)(int, int);
    using ClipCursorFn = BOOL(WINAPI*)(const RECT*);

    static inline SetCursorPosFn oSetCursorPos = nullptr;
    static inline ClipCursorFn oClipCursor = nullptr;
    static inline bool installed = false;

    static BOOL WINAPI hkSetCursorPos(int X, int Y) {
        if (Menu::visible) return TRUE;
        return oSetCursorPos(X, Y);
    }

    static BOOL WINAPI hkClipCursor(const RECT* lpRect) {
        if (Menu::visible) return oClipCursor(nullptr);
        return oClipCursor(lpRect);
    }
};
