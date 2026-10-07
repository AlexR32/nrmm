#pragma once

#include <windows.h>
#include <string>
#include <fstream>
#include <mutex>
#include <vector>
#include <format>
#include <utility>
#include <atomic>
#include <cstddef>
#include "libs/imgui/imgui.h"

class Logger {
public:
    enum class Color : WORD {
        Default = 7, // FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE
        Red = FOREGROUND_RED | FOREGROUND_INTENSITY,
        Green = FOREGROUND_GREEN | FOREGROUND_INTENSITY,
        Blue = FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        Yellow = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
        Cyan = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        Magenta = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        White = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        Gray = FOREGROUND_INTENSITY,
    };

private:
    struct Entry {
        std::string text;
        Color color;
    };

    static constexpr const char* kWindowName = "Debug Console";
    static constexpr ImVec2 kWindowPos = ImVec2(60.0f, 540.0f);

    static constexpr std::size_t kMaxEntries = 2000;

    static inline std::vector<Entry> entries;
    static inline std::atomic_bool visible{false};
    static inline bool autoScroll = true;
    static inline std::size_t lastRenderedCount = 0;

    static inline std::ofstream logFile;
    static inline std::wstring logFilePath;
    static inline std::mutex logMutex;

    static std::string GetTimestamp();
    static std::string WideToUtf8(const wchar_t* message);
    static ImVec4 ToImVec4(Color color);
    static void WriteToFile(const char* message);
    static void WriteToFile(const wchar_t* message);

public:
    Logger() = delete;

    // Opens "<logDirectory>\<fileName>.log" for writing, purging any existing contents.
    // The path is kept wide so directories containing non-ASCII (e.g. Cyrillic) characters work.
    // When logDirectory is empty the current working directory is used.
    static void Initialize(const std::wstring& logDirectory, const std::wstring& fileName);
    static void Cleanup();

    static void SetVisibility(bool visible);
    static bool GetVisibility();

    static const std::wstring& GetLogFilePath();

    static void Clear();

    // Draws the in-game console window. Must be called from the render thread.
    // Safe to call every frame; it does nothing while hidden.
    static void Render();

    // Simple logging narrow
    static void Log(const char* message, Color color = Color::Default);
    static void Log(const std::string& message, Color color = Color::Default);

    // Simple logging wide
    static void Log(const wchar_t* message, Color color = Color::Default);
    static void Log(const std::wstring& message, Color color = Color::Default);

    // Formatted logging narrow
    template<typename... Args>
    static void Logf(Color color, const char* formatStr, Args&&... args) {
        try {
            Log(std::vformat(formatStr, std::make_format_args(args...)), color);
        } catch (const std::format_error& e) {
            Log(std::string("Format error: ") + e.what(), Color::Red);
        }
    }

    template<typename... Args>
    static void Logf(const char* formatStr, Args&&... args) {
        Logf(Color::Default, formatStr, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Logf(Color color, const std::string& formatStr, Args&&... args) {
        Logf(color, formatStr.c_str(), std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Logf(const std::string& formatStr, Args&&... args) {
        Logf(Color::Default, formatStr.c_str(), std::forward<Args>(args)...);
    }

    // Formatted logging wide
    template<typename... Args>
    static void Logf(Color color, const wchar_t* formatStr, Args&&... args) {
        try {
            Log(std::vformat(formatStr, std::make_wformat_args(args...)), color);
        } catch (const std::format_error& e) {
            Log(std::string("Format error: ") + e.what(), Color::Red);
        }
    }

    template<typename... Args>
    static void Logf(const wchar_t* formatStr, Args&&... args) {
        Logf(Color::Default, formatStr, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Logf(Color color, const std::wstring& formatStr, Args&&... args) {
        Logf(color, formatStr.c_str(), std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Logf(const std::wstring& formatStr, Args&&... args) {
        Logf(Color::Default, formatStr.c_str(), std::forward<Args>(args)...);
    }
};
