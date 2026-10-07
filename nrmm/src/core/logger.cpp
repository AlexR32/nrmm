#include <pch.h>
#include "logger.h"
#include <ctime>

ImVec4 Logger::ToImVec4(Color color) {
    switch (color) {
    case Color::Red: return ImVec4(1.00f, 0.35f, 0.35f, 1.0f);
    case Color::Green: return ImVec4(0.40f, 1.00f, 0.40f, 1.0f);
    case Color::Blue: return ImVec4(0.45f, 0.60f, 1.00f, 1.0f);
    case Color::Yellow: return ImVec4(1.00f, 0.90f, 0.35f, 1.0f);
    case Color::Cyan: return ImVec4(0.40f, 1.00f, 1.00f, 1.0f);
    case Color::Magenta: return ImVec4(1.00f, 0.50f, 1.00f, 1.0f);
    case Color::White: return ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    case Color::Gray: return ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
    case Color::Default:
    default: return ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
    }
}

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

std::string Logger::WideToUtf8(const wchar_t* message) {
    if (!message) return {};

    const int size = WideCharToMultiByte(CP_UTF8, 0, message, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};

    std::string utf8(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, message, -1, utf8.data(), size, nullptr, nullptr);
    utf8.resize(static_cast<std::size_t>(size) - 1);
    return utf8;
}

void Logger::WriteToFile(const char* message) {
    if (!logFile.is_open() || !message) return;

    logFile << GetTimestamp() << message << '\n';
    logFile.flush();
}

void Logger::WriteToFile(const wchar_t* message) {
    if (!logFile.is_open() || !message) return;

    const std::string utf8 = WideToUtf8(message);
    if (utf8.empty()) return;

    logFile << GetTimestamp() << utf8 << '\n';
    logFile.flush();
}

void Logger::Initialize(const std::wstring& logDirectory, const std::wstring& fileName) {
    std::lock_guard<std::mutex> lock(logMutex);

    if (!logFile.is_open()) {
        std::wstring directory = logDirectory;
        if (directory.empty()) {
            directory = L".";
        } else if (directory.back() != L'\\' && directory.back() != L'/') {
            directory += L'\\';
        }

        logFilePath = directory + fileName + L".log";

        // MSVC's ofstream accepts a wide path, so non-ASCII directories resolve.
        logFile.open(logFilePath.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
        if (logFile.is_open()) {
            static const char kUtf8Bom[] = "\xEF\xBB\xBF";
            logFile.write(kUtf8Bom, 3);
            logFile.flush();
        }
    }
}

void Logger::Cleanup() {
    std::lock_guard<std::mutex> lock(logMutex);

    if (logFile.is_open()) {
        logFile.flush();
        logFile.close();
    }
    logFilePath.clear();

    entries.clear();
    lastRenderedCount = 0;
    visible.store(false, std::memory_order_relaxed);
}

void Logger::SetVisibility(bool show) {
    visible.store(show, std::memory_order_relaxed);
}

bool Logger::GetVisibility() {
    return visible.load(std::memory_order_relaxed);
}

const std::wstring& Logger::GetLogFilePath() {
    return logFilePath;
}

void Logger::Clear() {
    std::lock_guard<std::mutex> lock(logMutex);
    entries.clear();
    lastRenderedCount = 0;
}

// Simple logging narrow

void Logger::Log(const char* message, Color color) {
    if (!message) return;
    std::lock_guard<std::mutex> lock(logMutex);

    entries.push_back({GetTimestamp() + message, color});
    if (entries.size() > kMaxEntries)
        entries.erase(entries.begin(), entries.begin() + (entries.size() - kMaxEntries));

    WriteToFile(message);
}

void Logger::Log(const std::string& message, Color color) {
    Log(message.c_str(), color);
}

// Simple logging wide

void Logger::Log(const wchar_t* message, Color color) {
    if (!message) return;
    std::lock_guard<std::mutex> lock(logMutex);

    const std::string utf8 = WideToUtf8(message);
    if (utf8.empty()) return;

    entries.push_back({GetTimestamp() + utf8, color});
    if (entries.size() > kMaxEntries)
        entries.erase(entries.begin(), entries.begin() + (entries.size() - kMaxEntries));

    WriteToFile(message);
}

void Logger::Log(const std::wstring& message, Color color) {
    Log(message.c_str(), color);
}

void Logger::Render() {
    if (!visible.load(std::memory_order_relaxed)) return;

    ImGui::SetNextWindowPos(kWindowPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(640.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(kWindowName, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::Button("Clear")) {
            Clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy")) {
            std::string all;
            {
                std::lock_guard<std::mutex> lock(logMutex);
                for (const Entry& entry : entries) {
                    all += entry.text;
                    all += '\n';
                }
            }
            if (!all.empty())
                ImGui::SetClipboardText(all.c_str());
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &autoScroll);

        ImGui::Separator();

        ImGui::BeginChild("log_scroll", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
        {
            std::lock_guard<std::mutex> lock(logMutex);
            for (const Entry& entry : entries) {
                ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(entry.color));
                ImGui::TextUnformatted(entry.text.c_str());
                ImGui::PopStyleColor();
            }

            if (autoScroll && entries.size() != lastRenderedCount)
                ImGui::SetScrollHereY(1.0f);
            lastRenderedCount = entries.size();
        }
        ImGui::EndChild();
    }
    ImGui::End();
}
