#include <pch.h>
#include "logger.h"
#include <ctime>

std::string Logger::GetTimestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
    localtime_s(&localTime, &now);

    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d ",
        localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
        localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
    return std::string(buffer);
}

void Logger::WriteToFile(const char* message) {
    if (!logFile.is_open() || !message) return;

    logFile << GetTimestamp() << message << '\n';
    logFile.flush();
}

void Logger::WriteToFile(const wchar_t* message) {
    if (!logFile.is_open() || !message) return;

    const int size = WideCharToMultiByte(CP_UTF8, 0, message, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return;

    std::string utf8(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, message, -1, utf8.data(), size, nullptr, nullptr);
    utf8.resize(static_cast<std::size_t>(size) - 1);

    logFile << GetTimestamp() << utf8 << '\n';
    logFile.flush();
}

void Logger::Initialize(const std::string& logDirectory, const std::string& fileName) {
    std::lock_guard<std::mutex> lock(logMutex);

    if (!consoleAllocated) {
        AllocConsole();
        consoleAllocated = true;

        consoleWnd = GetConsoleWindow();

        FILE* stream = nullptr;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);
        hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    if (!logFile.is_open()) {
        std::string directory = logDirectory;
        if (directory.empty()) {
            directory = ".";
        } else if (directory.back() != '\\' && directory.back() != '/') {
            directory += '\\';
        }

        logFilePath = directory + fileName + ".log";
        logFile.open(logFilePath, std::ios::out | std::ios::trunc);
    }
}

void Logger::Cleanup() {
    std::lock_guard<std::mutex> lock(logMutex);

    if (logFile.is_open()) {
        logFile.flush();
        logFile.close();
    }
    logFilePath.clear();

    if (consoleAllocated) {
        fclose(stdout);
        fclose(stderr);

        hConsole = nullptr;
        consoleWnd = nullptr;

        FreeConsole();
        consoleAllocated = false;
    }
}

void Logger::SetVisibility(bool visible) {
    if (!consoleWnd) return;

    ShowWindow(consoleWnd, visible ? SW_SHOW : SW_HIDE);
}

bool Logger::GetVisibility() {
    return consoleWnd && IsWindowVisible(consoleWnd) != FALSE;
}

void Logger::SetTitle(const char* title) {
    SetConsoleTitleA(title);
}

void Logger::SetTitle(const std::string& title) {
    SetConsoleTitleA(title.c_str());
}

void Logger::SetTitle(const std::wstring& title) {
    SetConsoleTitleW(title.c_str());
}

std::string Logger::GetTitleA() {
    char buffer[1024] = {0};
    GetConsoleTitleA(buffer, 1024);
    return std::string(buffer);
}

std::wstring Logger::GetTitleW() {
    wchar_t buffer[1024] = {0};
    GetConsoleTitleW(buffer, 1024);
    return std::wstring(buffer);
}

const std::string& Logger::GetLogFilePath() {
    return logFilePath;
}

// Simple logging narrow

void Logger::Log(const char* message, Color color) {
    if (!message) return;
    std::lock_guard<std::mutex> lock(logMutex);

    if (hConsole) {
        CONSOLE_SCREEN_BUFFER_INFO info;
        const bool hasInfo = GetConsoleScreenBufferInfo(hConsole, &info) != FALSE;

        SetConsoleTextAttribute(hConsole, static_cast<WORD>(color));
        std::printf("%s\n", message);
        if (hasInfo) SetConsoleTextAttribute(hConsole, info.wAttributes);
    } else {
        std::printf("%s\n", message);
    }

    WriteToFile(message);
}

void Logger::Log(const std::string& message, Color color) {
    Log(message.c_str(), color);
}

// Simple logging wide

void Logger::Log(const wchar_t* message, Color color) {
    if (!message) return;
    std::lock_guard<std::mutex> lock(logMutex);

    if (hConsole) {
        CONSOLE_SCREEN_BUFFER_INFO info;
        const bool hasInfo = GetConsoleScreenBufferInfo(hConsole, &info) != FALSE;

        SetConsoleTextAttribute(hConsole, static_cast<WORD>(color));
        std::wprintf(L"%s\n", message);
        if (hasInfo) SetConsoleTextAttribute(hConsole, info.wAttributes);
    } else {
        std::wprintf(L"%s\n", message);
    }

    WriteToFile(message);
}

void Logger::Log(const std::wstring& message, Color color) {
    Log(message.c_str(), color);
}
