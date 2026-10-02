#pragma once
#include "pch.h"
#include <atomic>

class InputBlock {
public:
    static void Install();
    static void Uninstall();

    static void Suppress();
    static void Release();

    static void WarpCursor(int x, int y);
    static void BindRenderThread();

    static void SetKeyboardBlock(bool enabled) {
        keyboardBlock.store(enabled, std::memory_order_relaxed);
    }

    static bool KeyboardBlock() {
        return keyboardBlock.load(std::memory_order_relaxed);
    }

private:
    using SetCursorPosFn = BOOL(WINAPI*)(int, int);
    using ClipCursorFn = BOOL(WINAPI*)(const RECT*);
    using GetAsyncKeyStateFn = SHORT(WINAPI*)(int);
    using GetKeyStateFn = SHORT(WINAPI*)(int);
    using GetKeyboardStateFn = BOOL(WINAPI*)(PBYTE);
    using GetRawInputDataFn = UINT(WINAPI*)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);
    using GetRawInputBufferFn = UINT(WINAPI*)(PRAWINPUT, PUINT, UINT);

    static bool HookExport(HMODULE module, const char* name, void* detour, void** original);

    static BOOL WINAPI hkSetCursorPos(int X, int Y);
    static BOOL WINAPI hkClipCursor(const RECT* lpRect);
    static SHORT WINAPI hkGetAsyncKeyState(int vKey);
    static SHORT WINAPI hkGetKeyState(int vKey);
    static BOOL WINAPI hkGetKeyboardState(PBYTE keys);
    static UINT WINAPI hkGetRawInputData(HRAWINPUT raw, UINT command, LPVOID data, PUINT size, UINT headerSize);
    static UINT WINAPI hkGetRawInputBuffer(PRAWINPUT data, PUINT size, UINT headerSize);

    static inline SetCursorPosFn oSetCursorPos = nullptr;
    static inline ClipCursorFn oClipCursor = nullptr;
    static inline GetAsyncKeyStateFn oGetAsyncKeyState = nullptr;
    static inline GetKeyStateFn oGetKeyState = nullptr;
    static inline GetKeyboardStateFn oGetKeyboardState = nullptr;
    static inline GetRawInputDataFn oGetRawInputData = nullptr;
    static inline GetRawInputBufferFn oGetRawInputBuffer = nullptr;

    static inline std::atomic_bool installed{false};
    static inline std::atomic_bool suppressed{false};
    static inline std::atomic_bool keyboardBlock{true};

    static inline thread_local bool renderThread = false;
};
