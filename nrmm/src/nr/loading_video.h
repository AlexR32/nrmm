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
    // Cached reflection handles for UnityEngine.Video.VideoPlayer
    struct VideoPlayerApi {
        Il2CppClass* klass = nullptr;
        const MethodInfo* setSource = nullptr;
        const MethodInfo* getSource = nullptr;
        const MethodInfo* setUrl = nullptr;
        const MethodInfo* setLoop = nullptr;
        const MethodInfo* getClip = nullptr;
    };

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

    inline static Shared::Status status{"LoadingVideo"};

    // File list, published by the render thread under listMutex
    inline static std::mutex listMutex;
    inline static std::vector<Shared::MediaFile> files;
    inline static std::atomic_bool listLoaded{false};

    // Selected file path, published to Pump() under pathMutex
    inline static std::mutex pathMutex;
    inline static std::string selectedPath;

    // Script-thread only
    inline static Il2CppObject* dashboard = nullptr;
    inline static Il2CppObject* player = nullptr;
    inline static Il2CppObject* clipSource = nullptr;
    inline static Il2CppObject* loadingSong = nullptr;
    inline static bool loadingSongHidden = false;
    inline static Il2CppObject* appliedPlayer = nullptr;
    inline static std::string appliedUri;
    inline static bool overrideApplied = false;
    inline static bool shutDown = false;

    // Random mode: the file chosen for the current loading screen, and the
    // combined "a load is running" state used to detect new loads
    inline static std::string chosenPath;
    inline static bool prevLoadActive = false;
};
