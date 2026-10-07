#include "pch.h"

#include "loading_video.h"
#include "core/config.h"
#include "globals.h"
#include "il2cpp/main_thread.h"
#include "core/logger.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <random>

Shared::Status LoadingVideo::status("LoadingVideo");
std::mutex LoadingVideo::listMutex;
std::vector<LoadingVideo::File> LoadingVideo::files;
std::atomic_bool LoadingVideo::listLoaded{ false };
std::mutex LoadingVideo::pathMutex;
std::string LoadingVideo::selectedPath;
Il2CppObject* LoadingVideo::dashboard = nullptr;
Il2CppObject* LoadingVideo::player = nullptr;
Il2CppObject* LoadingVideo::appliedPlayer = nullptr;
std::string LoadingVideo::appliedUri;
bool LoadingVideo::overrideApplied = false;
bool LoadingVideo::shutDown = false;
std::string LoadingVideo::chosenPath;
bool LoadingVideo::prevLoadingFlag = false;
Il2CppObject* LoadingVideo::prevClip = nullptr;

// Path and naming helpers

std::string LoadingVideo::WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), size, nullptr, nullptr);
    return out;
}

bool LoadingVideo::IsSupportedExtension(const std::wstring& extension) {
    std::wstring lower;
    lower.reserve(extension.size());
    for (wchar_t c : extension) lower += static_cast<wchar_t>(towlower(c));
    return lower == L".mp4" || lower == L".m4v" || lower == L".mov" || lower == L".webm";
}

// Unity's VideoPlayer.url accepts a file:/// URI. Paths may contain spaces or
// non-ASCII characters, so everything outside the URI unsafe set is
// percent-encoded
std::string LoadingVideo::ToFileUri(const std::string& utf8Path) {
    static const char* hex = "0123456789ABCDEF";

    std::string uri = "file:///";
    uri.reserve(utf8Path.size() + 8);

    for (unsigned char c : utf8Path) {
        if (c == '\\') {
            uri += '/';
        } else if (c == '/' || c == ':' || c == '-' || c == '_' || c == '.' || c == '~' ||
                   (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            uri += static_cast<char>(c);
        } else {
            uri += '%';
            uri += hex[c >> 4];
            uri += hex[c & 0x0F];
        }
    }
    return uri;
}

std::string LoadingVideo::FileName(const std::string& utf8Path) {
    const size_t slash = utf8Path.find_last_of("\\/");
    return slash == std::string::npos ? utf8Path : utf8Path.substr(slash + 1);
}

std::wstring LoadingVideo::FolderPath() {
    std::wstring directory = g_DllPath.empty() ? L"." : g_DllPath;
    if (!directory.empty() && directory.back() != L'\\' && directory.back() != L'/') directory += L'\\';
    return directory + L"videos";
}

// Reflection handles

LoadingVideo::VideoPlayerApi& LoadingVideo::Api() {
    static VideoPlayerApi api;
    if (!api.klass) {
        Il2CppClass* klass = Il2Cpp::FindClass("UnityEngine.Video.VideoPlayer", Il2Cpp::GetImage("UnityEngine.VideoModule"));
        if (klass) {
            api.klass = klass;
            api.setSource = Il2Cpp::GetMethod(klass, "set_source", 1);
            api.getSource = Il2Cpp::GetMethod(klass, "get_source", 0);
            api.setUrl = Il2Cpp::GetMethod(klass, "set_url", 1);
            api.setLoop = Il2Cpp::GetMethod(klass, "set_isLooping", 1);
            api.getClip = Il2Cpp::GetMethod(klass, "get_clip", 0);
        }
    }
    return api;
}

// File list

void LoadingVideo::Reload() {
    std::vector<File> found;

    std::error_code error;
    const std::wstring folder = FolderPath();
    std::filesystem::create_directories(folder, error);

    std::filesystem::directory_iterator iterator(folder, error);
    if (!error) {
        for (const std::filesystem::directory_entry& entry : iterator) {
            std::error_code entryError;
            if (!entry.is_regular_file(entryError)) continue;
            if (!IsSupportedExtension(entry.path().extension().wstring())) continue;

            File file;
            file.path = WideToUtf8(entry.path().wstring());
            file.name = WideToUtf8(entry.path().stem().wstring());
            found.push_back(std::move(file));
        }
    }

    std::sort(found.begin(), found.end(), [](const File& a, const File& b) { return a.name < b.name; });

    size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(listMutex);
        files = std::move(found);
        count = files.size();
    }
    listLoaded.store(true, std::memory_order_release);

    // Keep the configured file selected when it is still present, otherwise
    // fall back to the first entry (without overwriting the config)
    std::vector<File> snapshot;
    {
        std::lock_guard<std::mutex> lock(listMutex);
        snapshot = files;
    }

    int index = -1;
    const std::string saved = Config::LoadingVideoFile();
    for (int i = 0; i < static_cast<int>(snapshot.size()); ++i) {
        if (snapshot[i].path == saved) {
            index = i;
            break;
        }
    }
    if (index < 0 && !snapshot.empty()) index = 0;

    if (index >= 0) {
        std::lock_guard<std::mutex> lock(pathMutex);
        selectedPath = snapshot[index].path;
    } else {
        std::lock_guard<std::mutex> lock(pathMutex);
        selectedPath.clear();
    }

    status.Set(count == 0 ? "No videos in the videos folder" : "Found " + std::to_string(count) + " video(s)");
}

void LoadingVideo::EnsureLoaded() {
    if (listLoaded.load(std::memory_order_acquire)) return;
    Reload();
}

void LoadingVideo::Select(const std::string& path) {
    std::lock_guard<std::mutex> lock(pathMutex);
    selectedPath = path;
}

std::string LoadingVideo::PickRandom() {
    std::lock_guard<std::mutex> lock(listMutex);
    if (files.empty()) return {};

    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<size_t> dist(0, files.size() - 1);
    return files[dist(rng)].path;
}

// The game sets RCC_DashboardInputs.loading_loadingVideo while it loads the
// addressable clip and clears it once the clip is set, so a rising edge marks a
// new loading screen. A new clip replacing the old one is the fallback signal
// for loads fast enough to finish in a single frame
bool LoadingVideo::DetectNewLoad(Il2CppObject* target) {
    bool loadingFlag = false;
    if (dashboard) Il2Cpp::GetInstanceFieldValue(dashboard, "loading_loadingVideo", loadingFlag);

    Il2CppObject* clip = nullptr;
    VideoPlayerApi& api = Api();
    if (api.getClip && target) clip = Il2Cpp::Invoke(api.getClip, target, nullptr);

    const bool newLoad = (loadingFlag && !prevLoadingFlag) || (clip && clip != prevClip);
    prevLoadingFlag = loadingFlag;
    prevClip = clip;
    return newLoad;
}

// Managed object lookups

Il2CppObject* LoadingVideo::ResolveDashboard() {
    if (dashboard && Il2Cpp::IsUnityObjectAlive(dashboard)) return dashboard;

    Il2CppClass* klass = Il2Cpp::FindClass("RCC_DashboardInputs");
    dashboard = klass ? Il2Cpp::FindObjectOfType(klass) : nullptr;
    return dashboard;
}

Il2CppObject* LoadingVideo::ResolvePlayer() {
    if (player && Il2Cpp::IsUnityObjectAlive(player)) return player;

    Il2CppObject* resolved = ResolveDashboard();
    if (!resolved) return nullptr;

    player = Il2Cpp::GetInstanceFieldObject(resolved, "loading_videoPlayer");
    return player;
}

int LoadingVideo::CurrentSource(Il2CppObject* target) {
    VideoPlayerApi& api = Api();
    // Without the getter the value cannot be read; the game never touches
    // source itself, so treating it as already applied is safe
    if (!api.getSource || !target) return 1;
    return Il2Cpp::UnboxInt32(Il2Cpp::Invoke(api.getSource, target, nullptr), 1);
}

void LoadingVideo::Apply(Il2CppObject* target, const std::string& uri) {
    VideoPlayerApi& api = Api();
    if (!api.setUrl || !target) return;

    const bool newTarget = (appliedPlayer != target);

    // VideoSource.Url = 1
    if (newTarget || CurrentSource(target) != 1) {
        if (api.setSource) {
            int32_t source = 1;
            void* args[1] = { &source };
            Il2Cpp::Invoke(api.setSource, target, args);
        }
    }

    if (newTarget || appliedUri != uri) {
        Il2CppString* url = Il2Cpp::NewString(uri.c_str());
        void* args[1] = { url };
        Il2Cpp::Invoke(api.setUrl, target, args);

        // Loop so a clip shorter than the load never leaves a frozen frame
        if (api.setLoop) {
            bool loop = true;
            void* loopArgs[1] = { &loop };
            Il2Cpp::Invoke(api.setLoop, target, loopArgs);
        }

        appliedPlayer = target;
        appliedUri = uri;
        status.Set("Loading video: " + FileName(uri));
    }

    overrideApplied = true;
}

void LoadingVideo::Restore() {
    if (!overrideApplied) return;

    VideoPlayerApi& api = Api();
    if (player && Il2Cpp::IsUnityObjectAlive(player)) {
        if (api.setSource) {
            int32_t source = 0; // VideoSource.VideoClip
            void* args[1] = { &source };
            Il2Cpp::Invoke(api.setSource, player, args);
        }
        if (api.setUrl) {
            Il2CppString* empty = Il2Cpp::NewString("");
            void* args[1] = { empty };
            Il2Cpp::Invoke(api.setUrl, player, args);
        }
    }

    appliedPlayer = nullptr;
    appliedUri.clear();
    overrideApplied = false;
}

// Script-thread pump

void LoadingVideo::Pump() {
    Il2Cpp::ThreadAttach();
    if (shutDown) return;

    // Populate the file list and the default selection regardless of whether
    // the tab has ever been opened, so the override works from the first
    // loading screen
    EnsureLoaded();

    if (!Config::LoadingVideo()) {
        if (overrideApplied) {
            Restore();
            status.Set("Loading video restored to the game's clips");
        }
        return;
    }

    Il2CppObject* target = ResolvePlayer();
    if (!target) return;

    const bool random = Config::LoadingVideoRandom();

    std::string path;
    if (random) {
        // Pick a fresh clip at the start of every loading screen, mirroring the
        // game's own Random.Range(0, count) pick
        if (DetectNewLoad(target) || chosenPath.empty()) chosenPath = PickRandom();
        path = chosenPath;
    } else {
        chosenPath.clear();
        std::lock_guard<std::mutex> lock(pathMutex);
        path = selectedPath;
    }

    if (path.empty()) {
        if (overrideApplied) {
            Restore();
            status.Set("Loading video restored to the game's clips");
        }
        return;
    }

    Apply(target, ToFileUri(path));
}

void LoadingVideo::Shutdown() {
    if (shutDown) return;
    shutDown = true;

    Il2Cpp::ThreadAttach();
    Restore();
}

// UI

void LoadingVideo::RenderTab() {
    status.Render();

    if (!listLoaded.load(std::memory_order_acquire)) Reload();

    bool enabled = Config::LoadingVideo();
    if (ImGui::Checkbox("Replace loading screen video", &enabled)) Config::SetLoadingVideo(enabled);

    bool random = Config::LoadingVideoRandom();
    if (ImGui::Checkbox("Random video each load", &random)) Config::SetLoadingVideoRandom(random);

    if (ImGui::SmallButton("Reload folder")) Reload();
    ImGui::SameLine();

    std::vector<File> snapshot;
    std::string current;
    {
        std::lock_guard<std::mutex> lock(listMutex);
        snapshot = files;
    }
    {
        std::lock_guard<std::mutex> lock(pathMutex);
        current = selectedPath;
    }
    ImGui::TextDisabled("%d video(s)", static_cast<int>(snapshot.size()));

    ImGui::TextWrapped("Use an MP4 with H.264 video and AAC audio. Unity's Windows video player cannot decode VP9, AV1 or HEVC, so those files stay black.");

    if (snapshot.empty()) {
        ImGui::TextWrapped("Drop an .mp4 (H.264) file into the \"videos\" folder");
        return;
    }

    int selectedIndex = 0;
    for (int i = 0; i < static_cast<int>(snapshot.size()); ++i) {
        if (snapshot[i].path == current) {
            selectedIndex = i;
            break;
        }
    }

    ImGui::SetNextItemWidth(220.0f);
    ImGui::BeginDisabled(random);
    const char* preview = random ? "(random)" : snapshot[selectedIndex].name.c_str();
    if (ImGui::BeginCombo("##video", preview)) {
        for (int i = 0; i < static_cast<int>(snapshot.size()); ++i) {
            const bool selected = (i == selectedIndex);
            if (ImGui::Selectable(snapshot[i].name.c_str(), selected)) {
                Select(snapshot[i].path);
                Config::SetLoadingVideoFile(snapshot[i].path);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();

    if (!MainThread::MainThreadKnown()) {
        ImGui::TextDisabled("Waiting for the game thread...");
    }
}
