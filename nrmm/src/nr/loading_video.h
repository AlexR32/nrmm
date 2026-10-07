#pragma once
#include "pch.h"
#include "il2cpp/il2cpp.h"
#include "nr/shared.h"
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

// Loading tab: replaces the loading screen's intro video with a file the user
// drops into a "videos" folder next to nrmm.dll. A checkbox switches between
// always using the picked file and picking a random one each load, mirroring
// the random clip the game itself chooses.
//
// RCC_DashboardInputs.loadingScreen_playVideo picks a random addressable
// VideoClip and plays it on the private loading_videoPlayer VideoPlayer. The
// game only ever sets the clip on that player, never its source, so switching
// the player to VideoSource.Url and pointing url at our file is enough: the
// game's own coroutine keeps calling Play() each frame while the loading screen
// is up, so the custom clip starts as soon as the url is prepared. The game's
// own clips, catalog and bundles are never touched.
//
// Unity's Windows video player decodes through Media Foundation, so the file
// must be a codec it supports (H.264/AAC MP4). VP9, AV1 and HEVC-in-MP4 are
// not decoded by default and show a black loading screen.
//
// The render thread owns the widgets and only reads atomic state; managed
// objects are only touched on the game's script thread from Pump()

class LoadingVideo {
public:
    static void RenderTab();

    // Runs on the game's script thread once per frame from the GodConstant.Update
    // hook: keeps the loading player pointed at the selected file (or restores
    // the game's clip source when the override is off)
    static void Pump();

    // Runs on the script thread during teardown; hands the player back to the
    // game. Idempotent
    static void Shutdown();

private:
    struct File {
        std::string name;
        std::string path; // UTF-8 filesystem path
    };

    // Cached reflection handles for UnityEngine.Video.VideoPlayer
    struct VideoPlayerApi {
        Il2CppClass* klass = nullptr;
        const MethodInfo* setSource = nullptr;
        const MethodInfo* getSource = nullptr;
        const MethodInfo* setUrl = nullptr;
        const MethodInfo* setLoop = nullptr;
        const MethodInfo* getClip = nullptr;
    };

    // Folder next to the DLL that holds the user's video files
    static std::wstring FolderPath();

    // Rebuilds the file list. Filesystem only, so it is safe on any thread
    static void Reload();

    // Loads the file list once, on whichever thread needs it first. Populating
    // it must not depend on the tab being open, because Pump() runs from the
    // start of the game
    static void EnsureLoaded();

    // Publishes the picked file to Pump() and remembers it in the config
    static void Select(const std::string& path);

    // Random pick from the current file list, empty when there is none
    static std::string PickRandom();

    // True on the frame a new loading screen video starts, mirroring how the
    // game begins RCC_DashboardInputs.loadingScreen_playVideo
    static bool DetectNewLoad(Il2CppObject* target);

    static Il2CppObject* ResolveDashboard();
    static Il2CppObject* ResolvePlayer();

    // Points the player at the url and toggles source, url and looping only when
    // they changed
    static void Apply(Il2CppObject* player, const std::string& uri);
    static void Restore();
    static int CurrentSource(Il2CppObject* player);

    // Sets activeSelf on a component's (or GameObject's) GameObject
    static void SetActive(Il2CppObject* object, bool active);

    // loadingVideo_clipSource is the game's video source placeholder text; it is
    // hidden while our clip plays. The music "now playing" song text is the
    // UI_Text_loading_song child of loading_textMask, hidden the same way and
    // restored when the override is switched off
    static void HideClipSource();
    static void HideLoadingSong();
    static void RestoreLoadingSong();

    static VideoPlayerApi& Api();

    static std::string WideToUtf8(const std::wstring& value);
    static bool IsSupportedExtension(const std::wstring& extension);
    static std::string ToFileUri(const std::string& utf8Path);
    static std::string FileName(const std::string& utf8Path);

    static Shared::Status status;

    // File list, published by the render thread under listMutex
    static std::mutex listMutex;
    static std::vector<File> files;
    static std::atomic_bool listLoaded;

    // Selected file path, published to Pump() under pathMutex
    static std::mutex pathMutex;
    static std::string selectedPath;

    // Script-thread only
    static Il2CppObject* dashboard;
    static Il2CppObject* player;
    static Il2CppObject* clipSource;
    static Il2CppObject* loadingSong;
    static bool loadingSongHidden;
    static Il2CppObject* appliedPlayer;
    static std::string appliedUri;
    static bool overrideApplied;
    static bool shutDown;

    // Random mode: the file chosen for the current loading screen, and the
    // signals used to detect that a new one has started
    static std::string chosenPath;
    static bool prevLoadingFlag;
    static Il2CppObject* prevClip;
};
