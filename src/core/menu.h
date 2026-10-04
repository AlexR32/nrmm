#pragma once
#include "pch.h"
#include <atomic>

class Menu {
public:
    static inline std::atomic_bool initialized{false};
    static inline std::atomic_bool visible{false};

    static void Initialize();
    static void Shutdown();
private:
    struct InputMessage {
        HWND hWnd;
        UINT msg;
        WPARAM wParam;
        LPARAM lParam;
    };

    static void InitStyle();

    static void RenderMainWindow();
    static void RenderSettingsTab();
    static void RenderKeybind(const char* label, std::atomic_int& key, int target);
    static std::string KeyName(int vk);

    static void Render();
    static void PumpInput();
    static LRESULT HandleInput(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static bool ShouldForwardToImGui(UINT msg);
    static bool ShouldSwallow(UINT msg);
    static bool IsKeyboardMessage(UINT msg);

    static inline std::mutex inputMutex;
    static inline std::deque<InputMessage> inputQueue;
    static constexpr size_t kMaxQueuedInput = 512;

    static inline POINT savedCursor{};
    static inline bool hasSavedCursor = false;

    static inline std::atomic_int toggleKey{VK_INSERT};
    static inline std::atomic_int unloadKey{VK_DELETE};
    static inline std::atomic_int captureTarget{0};
};
