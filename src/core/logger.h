#pragma once

#include <windows.h>
#include <string>
#include <fstream>
#include <mutex>
#include <format>
#include <utility>

class Logger {
private:
    static inline bool consoleAllocated = false;
    static inline HANDLE hConsole = nullptr;
    static inline HWND consoleWnd = nullptr;

    static inline std::ofstream logFile;
    static inline std::wstring logFilePath;
    static inline std::mutex logMutex;

    static std::string GetTimestamp();
    static void WriteToFile(const char* message);
    static void WriteToFile(const wchar_t* message);
    static void WriteConsoleLine(const wchar_t* message);

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

public:
    Logger() = delete;

    // Allocates a console and opens "<logDirectory>\<fileName>.log" for writing, purging any existing contents.
    // The path is kept wide so directories containing non-ASCII (e.g. Cyrillic) characters work.
    // When logDirectory is empty the current working directory is used.
    static void Initialize(const std::wstring& logDirectory, const std::wstring& fileName);
    static void Cleanup();

    static void SetVisibility(bool visible);
    static bool GetVisibility();

    static void SetTitle(const char* title);
    static void SetTitle(const std::string& title);
    static void SetTitle(const std::wstring& title);

    static std::string GetTitleA();
    static std::wstring GetTitleW();

    static const std::wstring& GetLogFilePath();

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
