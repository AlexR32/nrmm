#pragma once
#include "pch.h"

// Persists the Settings tab state to "nrmm.ini" next to the loaded DLL.
class Config {
public:
    static void Load();
    static void Save();

    static bool BlockKeyboard() { return blockKeyboard.load(std::memory_order_relaxed); }
    static void SetBlockKeyboard(bool enabled);

    static bool DebugConsole() { return debugConsole.load(std::memory_order_relaxed); }
    static void SetDebugConsole(bool enabled);

    static int ToggleKey() { return toggleKey.load(std::memory_order_relaxed); }
    static void SetToggleKey(int vk);

    static int UnloadKey() { return unloadKey.load(std::memory_order_relaxed); }
    static void SetUnloadKey(int vk);

    // Loading-video tab: whether the loading screen video is replaced and the
    // UTF-8 path of the file it is replaced with
    static bool LoadingVideo() { return loadingVideo.load(std::memory_order_relaxed); }
    static void SetLoadingVideo(bool enabled);
    static bool LoadingVideoRandom() { return loadingVideoRandom.load(std::memory_order_relaxed); }
    static void SetLoadingVideoRandom(bool enabled);
    static std::string LoadingVideoFile();
    static void SetLoadingVideoFile(const std::string& path);

private:
    static std::wstring GetPath();

    static inline std::mutex fileMutex;
    static inline std::mutex valueMutex;

    static inline std::atomic_bool blockKeyboard{true};
    static inline std::atomic_bool debugConsole{false};
    static inline std::atomic_int toggleKey{VK_INSERT};
    static inline std::atomic_int unloadKey{VK_DELETE};
    static inline std::atomic_bool loadingVideo{false};
    static inline std::atomic_bool loadingVideoRandom{false};
    static inline std::string loadingVideoFile;
};
