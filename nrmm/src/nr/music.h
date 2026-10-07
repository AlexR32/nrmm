#pragma once
#include "pch.h"
#include "il2cpp/il2cpp.h"
#include "nr/shared.h"
#include <atomic>
#include <mutex>
#include <random>
#include <string>
#include <vector>

// Music tab: plays the user's own audio files through the game's own music
// system. Files are read from a "music" folder next to nrmm.dll, loaded at
// runtime with Unity's UnityWebRequest/AudioClip machinery and started on the
// PlanetJem.Audio.MusicPlayer AudioSource, so playback goes through the game's
// music mixer and honours the in-game audio settings and speed effects.
//
// The game's music state is deliberately left alone: GodConstant.Update only
// runs engineMusicSpeedFade (low-pass / high-pass / speed boost) while
// AudioManager.MusicState is Cruise or Race, so stopping the game's music makes
// those effects freeze. Instead the current source's clip is swapped for ours
// each frame while the state machine is kept quiet, and the game's track name
// is replaced with the file name.
//
// The render thread owns the widgets and only reads atomic state; everything
// that touches managed objects runs on the game's script thread from Pump()

class Music {
public:
    static void RenderTab();

    // Runs on the game's script thread once per frame from the GodConstant.Update
    // hook: drains a queued action, advances a runtime clip load and keeps the
    // game's music source playing our clip
    static void Pump();

    // Runs on the script thread during teardown; stops playback and releases the
    // clip and the game's music player. Idempotent
    static void Shutdown();

private:
    enum class Action : int { None = 0, Play, Stop, Pause, Resume, Next, Prev };

    struct Track {
        std::string name;
        std::string path; // UTF-8 filesystem path
    };

    // Cached reflection handles for the Unity and game types the tab needs
    struct AudioSourceApi {
        Il2CppClass* klass = nullptr;
        const MethodInfo* setClip = nullptr;
        const MethodInfo* getClip = nullptr;
        const MethodInfo* play = nullptr;
        const MethodInfo* stop = nullptr;
        const MethodInfo* pause = nullptr;
        const MethodInfo* isPlaying = nullptr;
        const MethodInfo* setLoop = nullptr;
        const MethodInfo* getTime = nullptr;
        const MethodInfo* setTime = nullptr;
    };

    struct RequestApi {
        Il2CppClass* klass = nullptr;
        const MethodInfo* isDone = nullptr;
        const MethodInfo* error = nullptr;
        const MethodInfo* send = nullptr;
        const MethodInfo* dispose = nullptr;
    };

    // Folder next to the DLL that holds the user's audio files
    static std::wstring FolderPath();

    // Rebuilds the track list. Filesystem only, so it is safe on the render thread
    static void Reload();

    // Queues an action for Pump(), called from the render thread
    static void Request(Action action, int index);

    // Script-thread action handlers
    static void HandleAction(Action action, int index);
    // resetPosition is false only when resuming the same track after a blocking
    // game state; every track switch (play, next/prev, phone keys, end-of-track)
    // starts from 0
    static void StartLoad(int index, bool resetPosition = true);
    static void FinishLoad();
    static void CancelLoad();
    static void ApplyPlayback();
    static void StopPlayback();

    // Ends the current track: advance the playlist or hand music back
    static void AdvanceOrStop();
    static float GetClipLength();

    // Full stop: hands music back to the game so its own tracks resume
    static void StopCustom();

    // Stops our clip and hands the source back to the game without clearing the
    // user's intent (used while a game state temporarily blocks custom music)
    static void ReleaseToGame();
    static void ResumeGameMusic();

    // Reads AudioManager.MusicState and suspends / resumes custom music when the
    // game enters or leaves a state it should own
    static void UpdateStateGate();
    static int CurrentMusicState();
    static bool IsStateAllowed(int state);
    // static bool IsAuctionOpen();
    // static bool IsCinematicActive();

    // Mirrors the in-game phone music double-press bindings (prev / next /
    // play-pause) onto the custom player while it owns the music source
    static void PollPhoneKeybinds();

    // Keeps the game's track monitor from advancing / selecting its own tracks
    // while our clip is playing
    static void BeginOverride();
    static void EndOverride();

    // Mirrors the current file name into the game's now-playing UI
    static void EnsureNowPlaying();
    static void ClearNowPlaying();

    static Il2CppObject* ResolveMusicPlayer();
    static Il2CppObject* ResolveAudioManager();
    static Il2CppObject* ResolveSource();

    static void SetClip(Il2CppObject* source, Il2CppObject* clip);
    static Il2CppObject* GetClip(Il2CppObject* source);
    static void PlaySource(Il2CppObject* source);
    static void StopSource(Il2CppObject* source);
    static void PauseSource(Il2CppObject* source);
    static bool SourceIsPlaying(Il2CppObject* source);
    static float GetSourceTime(Il2CppObject* source);
    static void SetSourceTime(Il2CppObject* source, float time);

    static std::string CurrentName();

    // Path and naming helpers
    static std::string WideToUtf8(const std::wstring& value);
    static bool IsSupportedExtension(const std::wstring& extension);
    static int AudioTypeForPath(const std::string& path);
    static std::string ToFileUri(const std::string& utf8Path);

    static int NextIndex(int current, int count, bool randomize);
    static int PrevIndex(int current, int count, bool randomize);

    static AudioSourceApi& AsApi();
    static RequestApi& RequestApiFor();

    static Shared::Status status;

    // Track list, published by the render thread under listMutex
    static std::mutex listMutex;
    static std::vector<Track> tracks;
    static std::atomic_bool listLoaded;

    // Render-thread widgets and queue
    static int selectedIndex;
    static std::atomic_bool playlist;
    static std::atomic_bool shuffle;
    static std::atomic<int> pendingAction;
    static std::atomic<int> pendingIndex;

    // Playback state published to the render thread
    static std::atomic_bool playing;
    static std::atomic_bool paused;
    static std::atomic_bool loading;
    static std::atomic<int> currentIndex;
    static std::mutex nameMutex;
    static std::string currentName;

    // Script-thread only
    static Il2CppObject* musicPlayer;
    static Il2CppObject* audioManager;
    static Il2CppObject* audioSource;
    static Il2CppObject* ourClip;
    static Il2CppObject* webRequest;
    // GC roots for ourClip / webRequest: the mod holds these across frames and
    // nothing in managed code necessarily references them, so without a handle
    // the GC can free them mid-use (crash on Invoke)
    static uint32_t ourClipHandle;
    static uint32_t webRequestHandle;
    static Il2CppObject* nowPlayingEntry;
    static std::string pushedName;
    // Playback position of the current custom track, kept across source swaps,
    // clip re-takes and game-state suspensions so music does not restart
    static float playbackTime;
    static bool overrideActive;
    // User intent: Play sets this, Stop clears it. When a game state blocks
    // custom music the intent stays set so playback resumes automatically
    static bool wantsCustom;
    static bool suspended;
    static bool shutDown;

    static std::mt19937 rng;
};
