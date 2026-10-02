#include "pch.h"
#include "input_block.h"

bool InputBlock::HookExport(HMODULE module, const char* name, void* detour, void** original) {
    if (!module || !name || !detour) return false;

    void* target = reinterpret_cast<void*>(GetProcAddress(module, name));
    if (!target) return false;

    if (MH_CreateHook(target, detour, original) != MH_OK) return false;

    if (MH_EnableHook(target) != MH_OK) {
        MH_RemoveHook(target);
        if (original) *original = nullptr;
        return false;
    }

    return true;
}

void InputBlock::Install() {
    if (installed.load(std::memory_order_acquire)) return;

    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (user32) {
        HookExport(user32, "SetCursorPos", reinterpret_cast<void*>(&hkSetCursorPos), reinterpret_cast<void**>(&oSetCursorPos));
        HookExport(user32, "ClipCursor", reinterpret_cast<void*>(&hkClipCursor), reinterpret_cast<void**>(&oClipCursor));
        HookExport(user32, "GetAsyncKeyState", reinterpret_cast<void*>(&hkGetAsyncKeyState), reinterpret_cast<void**>(&oGetAsyncKeyState));
        HookExport(user32, "GetKeyState", reinterpret_cast<void*>(&hkGetKeyState), reinterpret_cast<void**>(&oGetKeyState));
        HookExport(user32, "GetKeyboardState", reinterpret_cast<void*>(&hkGetKeyboardState), reinterpret_cast<void**>(&oGetKeyboardState));
        HookExport(user32, "GetRawInputData", reinterpret_cast<void*>(&hkGetRawInputData), reinterpret_cast<void**>(&oGetRawInputData));
        HookExport(user32, "GetRawInputBuffer", reinterpret_cast<void*>(&hkGetRawInputBuffer), reinterpret_cast<void**>(&oGetRawInputBuffer));
    }

    installed.store(true, std::memory_order_release);
}

void InputBlock::Uninstall() {
    if (!installed.exchange(false, std::memory_order_acq_rel)) return;

    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (user32) {
        struct HookEntry {
            const char* name;
            void** trampoline;
        };

        const HookEntry hooks[] = {
            { "SetCursorPos", reinterpret_cast<void**>(&oSetCursorPos) },
            { "ClipCursor", reinterpret_cast<void**>(&oClipCursor) },
            { "GetAsyncKeyState", reinterpret_cast<void**>(&oGetAsyncKeyState) },
            { "GetKeyState", reinterpret_cast<void**>(&oGetKeyState) },
            { "GetKeyboardState", reinterpret_cast<void**>(&oGetKeyboardState) },
            { "GetRawInputData", reinterpret_cast<void**>(&oGetRawInputData) },
            { "GetRawInputBuffer", reinterpret_cast<void**>(&oGetRawInputBuffer) },
        };

        for (const HookEntry& hook : hooks) {
            if (!*hook.trampoline) continue;

            void* target = reinterpret_cast<void*>(GetProcAddress(user32, hook.name));
            if (target) {
                MH_DisableHook(target);
                MH_RemoveHook(target);
            }
            *hook.trampoline = nullptr;
        }
    }

    suppressed.store(false, std::memory_order_release);
}

void InputBlock::Suppress() {
    if (suppressed.exchange(true, std::memory_order_acq_rel)) return;

    if (oClipCursor) oClipCursor(nullptr);
    else ClipCursor(nullptr);
}

void InputBlock::Release() {
    suppressed.store(false, std::memory_order_release);
}

void InputBlock::WarpCursor(int x, int y) {
    if (oSetCursorPos) oSetCursorPos(x, y);
    else SetCursorPos(x, y);
}

void InputBlock::BindRenderThread() {
    renderThread = true;
}

BOOL WINAPI InputBlock::hkSetCursorPos(int X, int Y) {
    if (suppressed.load(std::memory_order_relaxed)) return TRUE;
    if (!oSetCursorPos) return FALSE;
    return oSetCursorPos(X, Y);
}

BOOL WINAPI InputBlock::hkClipCursor(const RECT* lpRect) {
    if (suppressed.load(std::memory_order_relaxed)) return TRUE;
    if (!oClipCursor) return FALSE;
    return oClipCursor(lpRect);
}

SHORT WINAPI InputBlock::hkGetAsyncKeyState(int vKey) {
    if (suppressed.load(std::memory_order_relaxed) && keyboardBlock.load(std::memory_order_relaxed)) return 0;
    if (!oGetAsyncKeyState) return 0;
    return oGetAsyncKeyState(vKey);
}

SHORT WINAPI InputBlock::hkGetKeyState(int vKey) {
    if (suppressed.load(std::memory_order_relaxed) && keyboardBlock.load(std::memory_order_relaxed) && !renderThread) return 0;
    if (!oGetKeyState) return 0;
    return oGetKeyState(vKey);
}

BOOL WINAPI InputBlock::hkGetKeyboardState(PBYTE keys) {
    if (suppressed.load(std::memory_order_relaxed) && keyboardBlock.load(std::memory_order_relaxed) && !renderThread) {
        if (keys) ZeroMemory(keys, 256);
        return TRUE;
    }

    if (!oGetKeyboardState) return FALSE;
    return oGetKeyboardState(keys);
}

void InputBlock::NeutralizeRawInput(void* data) {
    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(data);
    if (!raw) return;

    if (raw->header.dwType == RIM_TYPEMOUSE) {
        raw->data.mouse.lLastX = 0;
        raw->data.mouse.lLastY = 0;
        raw->data.mouse.usButtonFlags = 0;
        raw->data.mouse.usButtonData = 0;
        raw->data.mouse.ulButtons = 0;
    } else if (raw->header.dwType == RIM_TYPEKEYBOARD && keyboardBlock.load(std::memory_order_relaxed)) {
        raw->data.keyboard.Flags |= RI_KEY_BREAK;
        raw->data.keyboard.Message = WM_KEYUP;
    }
}

UINT WINAPI InputBlock::hkGetRawInputData(HRAWINPUT raw, UINT command, LPVOID data, PUINT size, UINT headerSize) {
    if (!oGetRawInputData) return static_cast<UINT>(-1);

    const UINT result = oGetRawInputData(raw, command, data, size, headerSize);
    if (result != static_cast<UINT>(-1) && data && command == RID_INPUT
        && result >= sizeof(RAWINPUTHEADER) && suppressed.load(std::memory_order_relaxed)) {
        NeutralizeRawInput(data);
    }
    return result;
}

UINT WINAPI InputBlock::hkGetRawInputBuffer(PRAWINPUT data, PUINT size, UINT headerSize) {
    if (!oGetRawInputBuffer) return 0;

    const UINT count = oGetRawInputBuffer(data, size, headerSize);
    if (data && count > 0 && count != static_cast<UINT>(-1) && suppressed.load(std::memory_order_relaxed)) {
        BYTE* cursor = reinterpret_cast<BYTE*>(data);
        for (UINT i = 0; i < count; ++i) {
            RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(cursor);
            NeutralizeRawInput(raw);
            if (raw->header.dwSize < sizeof(RAWINPUTHEADER)) break;
            cursor += raw->header.dwSize;
        }
    }
    return count;
}
