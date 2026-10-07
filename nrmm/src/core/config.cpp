#include "pch.h"
#include "config.h"
#include "globals.h"
#include "input_block.h"
#include "logger.h"
#include <cstdlib>

namespace {
    std::string Trim(const std::string& value) {
        const std::size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return {};
        const std::size_t last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    bool ParseBool(const std::string& value, bool fallback) {
        if (value == "1" || value == "true" || value == "on") return true;
        if (value == "0" || value == "false" || value == "off") return false;
        return fallback;
    }

    bool ParseKey(const std::string& value, int& out) {
        char* end = nullptr;
        const long parsed = std::strtol(value.c_str(), &end, 10);
        if (end == value.c_str() || parsed < 1 || parsed > 254) return false;
        out = static_cast<int>(parsed);
        return true;
    }
}

std::wstring Config::GetPath() {
    std::wstring directory = g_DllPath.empty() ? L"." : g_DllPath;
    if (directory.back() != L'\\' && directory.back() != L'/') {
        directory += L'\\';
    }
    return directory + L"nrmm.ini";
}

void Config::Load() {
    std::lock_guard<std::mutex> lock(fileMutex);

    std::ifstream file(GetPath().c_str(), std::ios::in | std::ios::binary);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            const std::size_t separator = line.find('=');
            if (separator == std::string::npos) continue;

            const std::string key = Trim(line.substr(0, separator));
            const std::string value = Trim(line.substr(separator + 1));

            if (key == "block_keyboard") {
                blockKeyboard.store(ParseBool(value, true), std::memory_order_relaxed);
            } else if (key == "debug_console") {
                debugConsole.store(ParseBool(value, false), std::memory_order_relaxed);
            } else if (key == "toggle_key") {
                int vk = 0;
                if (ParseKey(value, vk)) toggleKey.store(vk, std::memory_order_relaxed);
            } else if (key == "unload_key") {
                int vk = 0;
                if (ParseKey(value, vk)) unloadKey.store(vk, std::memory_order_relaxed);
            } else if (key == "loading_video") {
                loadingVideo.store(ParseBool(value, false), std::memory_order_relaxed);
            } else if (key == "loading_video_random") {
                loadingVideoRandom.store(ParseBool(value, false), std::memory_order_relaxed);
            } else if (key == "loading_video_file") {
                std::lock_guard<std::mutex> valueLock(valueMutex);
                loadingVideoFile = value;
            }
        }
    }

    // Push the loaded (or default) state into the live systems.
    InputBlock::SetKeyboardBlock(BlockKeyboard());
    Logger::SetVisibility(DebugConsole());
}

void Config::Save() {
    std::lock_guard<std::mutex> lock(fileMutex);

    std::ofstream file(GetPath().c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
    if (!file.is_open()) return;

    file << "block_keyboard=" << (BlockKeyboard() ? 1 : 0) << '\n';
    file << "debug_console=" << (DebugConsole() ? 1 : 0) << '\n';
    file << "toggle_key=" << ToggleKey() << '\n';
    file << "unload_key=" << UnloadKey() << '\n';
    file << "loading_video=" << (LoadingVideo() ? 1 : 0) << '\n';
    file << "loading_video_random=" << (LoadingVideoRandom() ? 1 : 0) << '\n';
    file << "loading_video_file=" << LoadingVideoFile() << '\n';
    file.flush();
}

void Config::SetBlockKeyboard(bool enabled) {
    blockKeyboard.store(enabled, std::memory_order_relaxed);
    InputBlock::SetKeyboardBlock(enabled);
    Save();
}

void Config::SetDebugConsole(bool enabled) {
    debugConsole.store(enabled, std::memory_order_relaxed);
    Logger::SetVisibility(enabled);
    Save();
}

void Config::SetToggleKey(int vk) {
    toggleKey.store(vk, std::memory_order_relaxed);
    Save();
}

void Config::SetUnloadKey(int vk) {
    unloadKey.store(vk, std::memory_order_relaxed);
    Save();
}

void Config::SetLoadingVideo(bool enabled) {
    loadingVideo.store(enabled, std::memory_order_relaxed);
    Save();
}

void Config::SetLoadingVideoRandom(bool enabled) {
    loadingVideoRandom.store(enabled, std::memory_order_relaxed);
    Save();
}

std::string Config::LoadingVideoFile() {
    std::lock_guard<std::mutex> lock(valueMutex);
    return loadingVideoFile;
}

void Config::SetLoadingVideoFile(const std::string& path) {
    {
        std::lock_guard<std::mutex> lock(valueMutex);
        loadingVideoFile = path;
    }
    Save();
}
