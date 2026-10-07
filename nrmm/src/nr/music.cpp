#include "pch.h"

#include "music.h"
#include "globals.h"
#include "il2cpp/main_thread.h"
#include "core/logger.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>

Shared::Status Music::status("Music");
std::mutex Music::listMutex;
std::vector<Music::Track> Music::tracks;
std::atomic_bool Music::listLoaded{ false };
int Music::selectedIndex = 0;
std::atomic_bool Music::playlist{ true };
std::atomic_bool Music::shuffle{ false };
std::atomic<int> Music::pendingAction{ static_cast<int>(Action::None) };
std::atomic<int> Music::pendingIndex{ -1 };
std::atomic_bool Music::playing{ false };
std::atomic_bool Music::paused{ false };
std::atomic_bool Music::loading{ false };
std::atomic<int> Music::currentIndex{ -1 };
std::mutex Music::nameMutex;
std::string Music::currentName;
Il2CppObject* Music::musicPlayer = nullptr;
Il2CppObject* Music::audioManager = nullptr;
Il2CppObject* Music::audioSource = nullptr;
Il2CppObject* Music::ourClip = nullptr;
Il2CppObject* Music::webRequest = nullptr;
uint32_t Music::ourClipHandle = 0;
uint32_t Music::webRequestHandle = 0;
Il2CppObject* Music::nowPlayingEntry = nullptr;
std::string Music::pushedName;
float Music::playbackTime = 0.0f;
bool Music::overrideActive = false;
bool Music::wantsCustom = false;
bool Music::suspended = false;
bool Music::shutDown = false;
std::mt19937 Music::rng{ std::random_device{}() };

// Path and naming helpers

std::string Music::WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), size, nullptr, nullptr);
    return out;
}

bool Music::IsSupportedExtension(const std::wstring& extension) {
    std::wstring lower;
    lower.reserve(extension.size());
    for (wchar_t c : extension) lower += static_cast<wchar_t>(towlower(c));
    return lower == L".wav" || lower == L".mp3" || lower == L".ogg" || lower == L".aiff" || lower == L".aif";
}

// Unity's AudioType values, taken from the game's own enum
int Music::AudioTypeForPath(const std::string& path) {
    std::string lower = path;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower.size() >= 4) {
        const std::string tail = lower.substr(lower.size() - 4);
        if (tail == ".mp3") return 13; // MPEG
        if (tail == ".ogg") return 14; // OGGVORBIS
        if (tail == ".aif") return 2;  // AIFF
    }
    if (lower.size() >= 5 && lower.substr(lower.size() - 5) == ".aiff") return 2; // AIFF
    return 20; // WAV
}

// UnityWebRequest needs a URI, and local files use the file:/// scheme with
// forward slashes
std::string Music::ToFileUri(const std::string& utf8Path) {
    std::string uri = "file:///";
    uri.reserve(utf8Path.size() + 8);
    for (char c : utf8Path) uri += (c == '\\') ? '/' : c;
    return uri;
}

int Music::NextIndex(int current, int count, bool randomize) {
    if (count <= 1) return 0;
    if (randomize) {
        std::uniform_int_distribution<int> dist(0, count - 1);
        int next = dist(rng);
        if (next == current) next = (next + 1) % count;
        return next;
    }
    return (current + 1) % count;
}

int Music::PrevIndex(int current, int count, bool randomize) {
    if (count <= 1) return 0;
    if (randomize) {
        std::uniform_int_distribution<int> dist(0, count - 1);
        int next = dist(rng);
        if (next == current) next = (next + 1) % count;
        return next;
    }
    return (current - 1 + count) % count;
}

// Reflection handles

Music::AudioSourceApi& Music::AsApi() {
    static AudioSourceApi api;
    if (!api.klass) {
        Il2CppClass* klass = Il2Cpp::FindClass("UnityEngine.AudioSource", Il2Cpp::GetImage("UnityEngine.AudioModule"));
        if (klass) {
            api.klass = klass;
            api.setClip = Il2Cpp::GetMethod(klass, "set_clip", 1);
            api.getClip = Il2Cpp::GetMethod(klass, "get_clip", 0);
            api.play = Il2Cpp::GetMethod(klass, "Play", 0);
            api.stop = Il2Cpp::GetMethod(klass, "Stop", 0);
            api.pause = Il2Cpp::GetMethod(klass, "Pause", 0);
            api.isPlaying = Il2Cpp::GetMethod(klass, "get_isPlaying", 0);
            api.setLoop = Il2Cpp::GetMethod(klass, "set_loop", 1);
            api.getTime = Il2Cpp::GetMethod(klass, "get_time", 0);
            api.setTime = Il2Cpp::GetMethod(klass, "set_time", 1);
        }
    }
    return api;
}

Music::RequestApi& Music::RequestApiFor() {
    static RequestApi api;
    if (!api.klass) {
        Il2CppClass* klass = Il2Cpp::FindClass("UnityEngine.Networking.UnityWebRequest", Il2Cpp::GetImage("UnityEngine.UnityWebRequestModule"));
        if (klass) {
            api.klass = klass;
            api.isDone = Il2Cpp::GetMethod(klass, "get_isDone", 0);
            api.error = Il2Cpp::GetMethod(klass, "get_error", 0);
            api.send = Il2Cpp::GetMethod(klass, "SendWebRequest", 0);
            api.dispose = Il2Cpp::GetMethod(klass, "Dispose", 0);
        }
    }
    return api;
}

// Folder / track list

std::wstring Music::FolderPath() {
    std::wstring directory = g_DllPath.empty() ? L"." : g_DllPath;
    if (directory.back() != L'\\' && directory.back() != L'/') directory += L'\\';
    return directory + L"music";
}

void Music::Reload() {
    std::vector<Track> found;

    std::error_code error;
    const std::wstring folder = FolderPath();
    std::filesystem::create_directories(folder, error);

    std::filesystem::directory_iterator iterator(folder, error);
    if (!error) {
        for (const std::filesystem::directory_entry& entry : iterator) {
            std::error_code entryError;
            if (!entry.is_regular_file(entryError)) continue;
            if (!IsSupportedExtension(entry.path().extension().wstring())) continue;

            Track track;
            track.path = WideToUtf8(entry.path().wstring());
            track.name = WideToUtf8(entry.path().stem().wstring());
            found.push_back(std::move(track));
        }
    }

    std::sort(found.begin(), found.end(), [](const Track& a, const Track& b) { return a.name < b.name; });

    size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(listMutex);
        tracks = std::move(found);
        count = tracks.size();
    }
    listLoaded.store(true, std::memory_order_release);

    if (count == 0) {
        status.Set("No tracks in the music folder");
    } else {
        status.Set("Found " + std::to_string(count) + " track(s)");
    }
}

void Music::Request(Action action, int index) {
    pendingIndex.store(index, std::memory_order_relaxed);
    pendingAction.store(static_cast<int>(action), std::memory_order_release);
}

std::string Music::CurrentName() {
    std::lock_guard<std::mutex> lock(nameMutex);
    return currentName;
}

// Managed object lookups

Il2CppObject* Music::ResolveMusicPlayer() {
    if (musicPlayer && Il2Cpp::IsUnityObjectAlive(musicPlayer)) return musicPlayer;

    Il2CppClass* gameMaster = Il2Cpp::FindClass("PlanetJem.Core.GameMaster");
    if (gameMaster) {
        const MethodInfo* getter = Il2Cpp::GetMethod(gameMaster, "get_MusicPlayer", 0);
        if (getter) {
            if (Il2CppObject* player = Il2Cpp::Invoke(getter, nullptr, nullptr)) {
                if (Il2Cpp::IsUnityObjectAlive(player)) return player;
            }
        }
    }

    Il2CppClass* playerClass = Il2Cpp::FindClass("PlanetJem.Audio.MusicPlayer");
    return playerClass ? Il2Cpp::FindObjectOfType(playerClass) : nullptr;
}

Il2CppObject* Music::ResolveAudioManager() {
    if (audioManager && Il2Cpp::IsUnityObjectAlive(audioManager)) return audioManager;

    Il2CppClass* gameMaster = Il2Cpp::FindClass("PlanetJem.Core.GameMaster");
    if (gameMaster) {
        const MethodInfo* getter = Il2Cpp::GetMethod(gameMaster, "get_Audio", 0);
        if (getter) {
            if (Il2CppObject* manager = Il2Cpp::Invoke(getter, nullptr, nullptr)) {
                if (Il2Cpp::IsUnityObjectAlive(manager)) return manager;
            }
        }
    }

    Il2CppClass* managerClass = Il2Cpp::FindClass("PlanetJem.Audio.AudioManager");
    return managerClass ? Il2Cpp::FindObjectOfType(managerClass) : nullptr;
}

Il2CppObject* Music::ResolveSource() {
    Il2CppObject* player = ResolveMusicPlayer();
    if (!player) return nullptr;

    Il2CppObject* source = Il2Cpp::GetInstanceFieldObject(player, "currentSource_");
    if (!source) source = Il2Cpp::GetInstanceFieldObject(player, "sourceA_");
    if (!source) source = Il2Cpp::GetInstanceFieldObject(player, "sourceB_");
    return source;
}

void Music::SetClip(Il2CppObject* source, Il2CppObject* clip) {
    AudioSourceApi& api = AsApi();
    if (!api.setClip || !source) return;
    void* args[1] = { clip };
    Il2Cpp::Invoke(api.setClip, source, args);
}

Il2CppObject* Music::GetClip(Il2CppObject* source) {
    AudioSourceApi& api = AsApi();
    if (!api.getClip || !source) return nullptr;
    return Il2Cpp::Invoke(api.getClip, source, nullptr);
}

void Music::PlaySource(Il2CppObject* source) {
    AudioSourceApi& api = AsApi();
    if (api.play && source) Il2Cpp::Invoke(api.play, source, nullptr);
}

void Music::StopSource(Il2CppObject* source) {
    AudioSourceApi& api = AsApi();
    if (api.stop && source) Il2Cpp::Invoke(api.stop, source, nullptr);
}

void Music::PauseSource(Il2CppObject* source) {
    AudioSourceApi& api = AsApi();
    if (api.pause && source) Il2Cpp::Invoke(api.pause, source, nullptr);
}

bool Music::SourceIsPlaying(Il2CppObject* source) {
    AudioSourceApi& api = AsApi();
    if (!api.isPlaying || !source) return false;
    return Il2Cpp::UnboxBool(Il2Cpp::Invoke(api.isPlaying, source, nullptr));
}

float Music::GetSourceTime(Il2CppObject* source) {
    AudioSourceApi& api = AsApi();
    if (!api.getTime || !source) return 0.0f;
    void* raw = Il2Cpp::UnboxRaw(Il2Cpp::Invoke(api.getTime, source, nullptr));
    return raw ? *reinterpret_cast<float*>(raw) : 0.0f;
}

void Music::SetSourceTime(Il2CppObject* source, float time) {
    AudioSourceApi& api = AsApi();
    if (!api.setTime || !source) return;
    void* args[1] = { &time };
    Il2Cpp::Invoke(api.setTime, source, args);
}

// Override control

void Music::BeginOverride() {
    Il2CppObject* player = ResolveMusicPlayer();
    if (!player) return;
    if (!overrideActive) {
        // EnsureNowPlaying replaces the game's NowPlaying entry, which makes the
        // track monitor exit, so the game does not advance to its own next track
        // underneath us. loopCurrentTrack_ is deliberately NOT set: the game can
        // mirror it onto AudioSource.loop, and then our clip loops instead of
        // ending, so it would never advance
        overrideActive = true;
    }
}

void Music::EndOverride() {
    overrideActive = false;
}

void Music::EnsureNowPlaying() {
    Il2CppObject* player = ResolveMusicPlayer();
    if (!player) return;
    musicPlayer = player;

    const std::string name = CurrentName();

    // Only touch the overlay when the game replaced our entry or the track
    // changed, so the ticker animation is not restarted every frame
    Il2CppObject* current = Il2Cpp::GetInstanceFieldObject(player, "<NowPlaying>k__BackingField");
    const bool ours = current && current == nowPlayingEntry;
    if (ours && name == pushedName) return;

    if (!nowPlayingEntry) {
        Il2CppClass* entryClass = Il2Cpp::FindClass("PlanetJem.Audio.MusicTrackEntry");
        if (!entryClass) return;
        nowPlayingEntry = Il2Cpp::NewObject(entryClass);
        if (!nowPlayingEntry) return;
    }

    Il2CppString* nameString = Il2Cpp::NewString(name.c_str());
    Il2Cpp::SetInstanceFieldObject(nowPlayingEntry, "TrackName", nameString);
    Il2Cpp::SetInstanceFieldObject(player, "<NowPlaying>k__BackingField", nowPlayingEntry);
    pushedName = name;

    Il2CppClass* playerClass = Il2Cpp::FindClass("PlanetJem.Audio.MusicPlayer");
    const MethodInfo* refresh = playerClass ? Il2Cpp::GetMethod(playerClass, "RefreshNowPlayingOverlay", 1) : nullptr;
    if (refresh) {
        bool show = true;
        void* args[1] = { &show };
        Il2Cpp::Invoke(refresh, player, args);
    }
}

void Music::ClearNowPlaying() {
    if (musicPlayer && Il2Cpp::IsUnityObjectAlive(musicPlayer)) {
        Il2Cpp::SetInstanceFieldObject(musicPlayer, "<NowPlaying>k__BackingField", nullptr);

        Il2CppClass* playerClass = Il2Cpp::FindClass("PlanetJem.Audio.MusicPlayer");
        const MethodInfo* refresh = playerClass ? Il2Cpp::GetMethod(playerClass, "RefreshNowPlayingOverlay", 1) : nullptr;
        if (refresh) {
            bool show = false;
            void* args[1] = { &show };
            Il2Cpp::Invoke(refresh, musicPlayer, args);
        }
    }
    nowPlayingEntry = nullptr;
    pushedName.clear();
}

void Music::ResumeGameMusic() {
    Il2CppObject* manager = ResolveAudioManager();
    if (!manager) return;

    Il2CppClass* managerClass = Il2Cpp::FindClass("PlanetJem.Audio.AudioManager");
    if (!managerClass) return;

    const MethodInfo* getState = Il2Cpp::GetMethod(managerClass, "get_MusicState", 0);
    const MethodInfo* playMusic = Il2Cpp::GetMethod(managerClass, "PlayMusic", 2);
    if (!playMusic) return;

    int32_t state = 0;
    if (getState) state = Il2Cpp::UnboxInt32(Il2Cpp::Invoke(getState, manager, nullptr));
    if (state == 0) return; // Off: nothing was playing to resume

    bool useFade = true;
    void* args[2] = { &state, &useFade };
    Il2Cpp::Invoke(playMusic, manager, args);
}

int Music::CurrentMusicState() {
    Il2CppObject* manager = ResolveAudioManager();
    if (!manager) return 0;

    Il2CppClass* managerClass = Il2Cpp::FindClass("PlanetJem.Audio.AudioManager");
    static const MethodInfo* getState = nullptr;
    if (!getState) getState = managerClass ? Il2Cpp::GetMethod(managerClass, "get_MusicState", 0) : nullptr;
    if (!getState) return 0;

    return Il2Cpp::UnboxInt32(Il2Cpp::Invoke(getState, manager, nullptr));
}

// MusicPlayerState: Off=0, Loading=1, Garage=2, Meetspot=3, Cruise=4, Race=5,
// RaceWin=6, RaceLoss=7, MainMenu=8. Custom music only replaces the ambient
// gameplay tracks. Off is blocked too: AudioManager.StopMusic (the primitive the
// race intro/outro cinematics, loading screen and main menu use) drives the
// state to Off, so Off means the game deliberately silenced music
bool Music::IsStateAllowed(int state) {
    switch (state) {
    case 2: // Garage
    case 3: // Meetspot
    case 4: // Cruise
    case 5: // Race
        return true;
    default:
        return false;
    }
}

// Auction keeps MusicState at Garage, so it needs its own check. The auction
// object stays referenced after it is switched off, so only its active state
// tells whether it is really open (same signal the Auction tab uses)
/*bool Music::IsAuctionOpen() {
    Il2CppObject* carAuction = Shared::CarAuction();
    return carAuction != nullptr && Il2Cpp::IsActiveSelf(carAuction);
}*/

// The race intro (raceSpot._race_carsIntro) and race finish set
// GodConstant.cinematic_playing while they run AudioManager.StopMusic and the
// cinematic camera; race_countdown_2 clears it again. Blocking on it keeps
// custom music out of cinematics even if a state transition lags
/*bool Music::IsCinematicActive() {
    Il2CppObject* god = Shared::God();
    if (!god) return false;

    bool cinematic = false;
    return Il2Cpp::GetInstanceFieldValue(god, "cinematic_playing", cinematic) && cinematic;
}*/

void Music::UpdateStateGate() {
    if (!wantsCustom) return;

    const bool blocked = !IsStateAllowed(CurrentMusicState());

    if (blocked) {
        if (!suspended) {
            ReleaseToGame();
            suspended = true;
            status.Set("Custom music paused for this game state");
        }
    } else if (suspended) {
        musicPlayer = ResolveMusicPlayer();
        audioSource = ResolveSource();
        if (!audioSource) {
            status.Set("Music source not available");
            return;
        }
        suspended = false;
        StartLoad(currentIndex.load(std::memory_order_relaxed) >= 0 ? currentIndex.load(std::memory_order_relaxed) : selectedIndex, false);
    }
}

// Playback

void Music::StopPlayback() {
    if (audioSource && Il2Cpp::IsUnityObjectAlive(audioSource)) {
        StopSource(audioSource);
        SetClip(audioSource, nullptr);
    }
    if (ourClip) {
        Shared::DestroyObject(ourClip);
        ourClip = nullptr;
    }
    if (ourClipHandle) {
        Il2Cpp::GcHandleFree(ourClipHandle);
        ourClipHandle = 0;
    }
    playing.store(false, std::memory_order_release);
    paused.store(false, std::memory_order_release);
}

void Music::ReleaseToGame() {
    StopPlayback();
    EndOverride();
    ClearNowPlaying();
    ResumeGameMusic();
}

void Music::StopCustom() {
    wantsCustom = false;
    suspended = false;
    playbackTime = 0.0f;
    ReleaseToGame();
    status.Set("Stopped");
}

// The in-game phone binds double-press actions 202 (previous), 203 (next) and
// 201 (play/pause) and acts on them in GodConstant.Update. Because our hook
// runs first we read the same state and, while our clip owns the source,
// consume the press so the game's handler does not also crossfade to its own
// track
void Music::PollPhoneKeybinds() {
    Il2CppObject* god = Shared::God();
    if (!god) return;

    bool phoneActive = false;
    if (!Il2Cpp::GetInstanceFieldValue(god, "phoneIsActive", phoneActive) || !phoneActive) return;

    Il2CppClass* inputClass = Il2Cpp::FindClass("PlanetJem.Input.GameInput");
    if (!inputClass) return;
    const MethodInfo* doublePressed = Il2Cpp::GetMethod(inputClass, "DoublePressed", 1);
    if (!doublePressed) return;

    Il2CppObject* state = Il2Cpp::GetStaticFieldObject(inputClass, "state_");
    if (!state) return;
    Il2CppObject* pressedArray = Il2Cpp::GetInstanceFieldObject(state, "doublePressed_");
    if (!pressedArray) return;

    uint8_t* elements = reinterpret_cast<uint8_t*>(pressedArray) + Il2Cpp::kArrayDataOffset;

    auto consume = [&](int actionId, Action action) {
        int32_t id = actionId;
        void* args[1] = { &id };
        if (!Il2Cpp::UnboxBool(Il2Cpp::Invoke(doublePressed, nullptr, args))) return;
        elements[actionId] = 0;
        HandleAction(action, -1);
    };

    consume(202, Action::Prev);
    consume(203, Action::Next);

    int32_t toggleId = 201;
    void* toggleArgs[1] = { &toggleId };
    if (Il2Cpp::UnboxBool(Il2Cpp::Invoke(doublePressed, nullptr, toggleArgs))) {
        elements[201] = 0;
        if (playing.load(std::memory_order_relaxed)) {
            HandleAction(paused.load(std::memory_order_relaxed) ? Action::Resume : Action::Pause, -1);
        }
    }
}

void Music::CancelLoad() {
    if (webRequest) {
        RequestApi& api = RequestApiFor();
        if (api.dispose) Il2Cpp::Invoke(api.dispose, webRequest, nullptr);
        webRequest = nullptr;
    }
    if (webRequestHandle) {
        Il2Cpp::GcHandleFree(webRequestHandle);
        webRequestHandle = 0;
    }
    loading.store(false, std::memory_order_release);
}

void Music::StartLoad(int index, bool resetPosition) {
    Track track;
    size_t count = 0;
    {
        std::lock_guard<std::mutex> lock(listMutex);
        count = tracks.size();
        if (index < 0 || index >= static_cast<int>(count)) {
            status.Set("No track selected");
            return;
        }
        track = tracks[index];
    }

    if (resetPosition) playbackTime = 0.0f;

    currentIndex.store(index, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(nameMutex);
        currentName = track.name;
    }

    CancelLoad();

    Il2CppClass* media = Il2Cpp::FindClass("UnityEngine.Networking.UnityWebRequestMultimedia", Il2Cpp::GetImage("UnityEngine.UnityWebRequestAudioModule"));
    const MethodInfo* getAudioClip = media ? Il2Cpp::GetMethod(media, "GetAudioClip", 2) : nullptr;
    if (!getAudioClip) {
        status.Set("UnityWebRequestMultimedia.GetAudioClip not found");
        return;
    }

    const std::string uri = ToFileUri(track.path);
    int audioType = AudioTypeForPath(track.path);

    Il2CppString* uriString = Il2Cpp::NewString(uri.c_str());
    void* args[2] = { uriString, &audioType };
    Il2CppObject* request = Il2Cpp::Invoke(getAudioClip, nullptr, args);
    if (!request) {
        status.Set("Failed to create the audio request");
        return;
    }

    RequestApi& api = RequestApiFor();
    if (!api.send) {
        if (api.dispose) Il2Cpp::Invoke(api.dispose, request, nullptr);
        status.Set("UnityWebRequest.SendWebRequest not found");
        return;
    }
    Il2Cpp::Invoke(api.send, request, nullptr);

    webRequest = request;
    webRequestHandle = Il2Cpp::GcHandleNew(request);
    loading.store(true, std::memory_order_release);
    status.Set("Loading " + track.name + "...");
}

void Music::FinishLoad() {
    Il2CppObject* request = webRequest;
    if (!request) {
        loading.store(false, std::memory_order_release);
        return;
    }

    RequestApi& api = RequestApiFor();

    std::string error;
    if (api.error) {
        if (Il2CppObject* errorObject = Il2Cpp::Invoke(api.error, request, nullptr)) {
            error = Il2Cpp::StringToUtf8(reinterpret_cast<Il2CppString*>(errorObject));
        }
    }
    if (!error.empty()) {
        status.Set("Load failed: " + error);
        CancelLoad();
        return;
    }

    Il2CppClass* handler = Il2Cpp::FindClass("UnityEngine.Networking.DownloadHandlerAudioClip", Il2Cpp::GetImage("UnityEngine.UnityWebRequestAudioModule"));
    const MethodInfo* getContent = handler ? Il2Cpp::GetMethod(handler, "GetContent", 1) : nullptr;
    if (!getContent) {
        status.Set("DownloadHandlerAudioClip.GetContent not found");
        CancelLoad();
        return;
    }

    void* args[1] = { request };
    Il2CppObject* clip = Il2Cpp::Invoke(getContent, nullptr, args);
    if (!clip) {
        status.Set("Audio clip could not be decoded");
        CancelLoad();
        return;
    }

    if (!audioSource || !Il2Cpp::IsUnityObjectAlive(audioSource)) {
        Shared::DestroyObject(clip);
        status.Set("Music source is gone");
        CancelLoad();
        return;
    }

    if (ourClip) {
        Shared::DestroyObject(ourClip);
        Il2Cpp::GcHandleFree(ourClipHandle);
        ourClip = nullptr;
        ourClipHandle = 0;
    }
    ourClip = clip;
    ourClipHandle = Il2Cpp::GcHandleNew(clip);

    AudioSourceApi& as = AsApi();
    if (as.setLoop) {
        bool loop = false;
        void* loopArgs[1] = { &loop };
        Il2Cpp::Invoke(as.setLoop, audioSource, loopArgs);
    }

    // The game's MusicPlayer.Update writes currentSource_.volume from
    // currentVolume_ * volumeScale_ every frame, so bring currentVolume_ to full
    // once and let the game's own mixer do the rest (volume slider, speed fade)
    // Il2CppObject* player = ResolveMusicPlayer();
    // if (player) {
    //     musicPlayer = player;
    //     Il2Cpp::SetInstanceFieldValue(player, "currentVolume_", 1.0f);
    // }

    SetClip(audioSource, ourClip);
    PlaySource(audioSource);
    // Always set the position, including 0: the game's crossfade can leave its
    // own time on the source, and Play() would otherwise start from there
    SetSourceTime(audioSource, playbackTime);

    BeginOverride();
    EnsureNowPlaying();

    CancelLoad();
    playing.store(true, std::memory_order_release);
    paused.store(false, std::memory_order_release);
    status.Set("Playing " + CurrentName());
}

void Music::ApplyPlayback() {
    if (!ourClip) {
        playing.store(false, std::memory_order_release);
        return;
    }

    Il2CppObject* source = ResolveSource();
    if (!source) return;

    // On a crossfade the game swaps the active source. Follow it and silence the
    // one we were using so two tracks never overlap
    if (source != audioSource) {
        if (audioSource && Il2Cpp::IsUnityObjectAlive(audioSource)) StopSource(audioSource);
        audioSource = source;
    }

    if (!Il2Cpp::IsUnityObjectAlive(audioSource)) {
        audioSource = nullptr;
        return;
    }

    BeginOverride();

    // Force loop off on every frame we own the source: the game may have set it
    // while loopCurrentTrack_ was active or when it started its own track, and a
    // looping source never reports !isPlaying so the track would repeat
    AudioSourceApi& loopApi = AsApi();
    if (loopApi.setLoop) {
        bool loop = false;
        void* loopArgs[1] = { &loop };
        Il2Cpp::Invoke(loopApi.setLoop, audioSource, loopArgs);
    }

    const float length = GetClipLength();
    const bool finished = length > 0.0f && playbackTime >= length - 0.15f;

    if (GetClip(audioSource) != ourClip) {
        // The game swapped the source or replaced our clip. If our track had
        // already reached its end, advance instead of replaying it; otherwise
        // take the source back at the position we had reached. Setting the time
        // unconditionally clears whatever the game left on the source
        if (finished) {
            AdvanceOrStop();
            return;
        }
        SetClip(audioSource, ourClip);
        PlaySource(audioSource);
        SetSourceTime(audioSource, playbackTime);
        EnsureNowPlaying();
        return;
    }

    EnsureNowPlaying();

    if (!SourceIsPlaying(audioSource)) {
        if (paused.load(std::memory_order_relaxed)) return;
        AdvanceOrStop();
        return;
    }

    playbackTime = GetSourceTime(audioSource);
}

// Ends the current track: play the next one when the playlist is on, otherwise
// hand the source back to the game
void Music::AdvanceOrStop() {
    playbackTime = 0.0f;

    if (playlist.load(std::memory_order_relaxed)) {
        size_t count;
        {
            std::lock_guard<std::mutex> lock(listMutex);
            count = tracks.size();
        }
        if (count > 0) {
            StartLoad(NextIndex(currentIndex.load(std::memory_order_relaxed), static_cast<int>(count), shuffle.load(std::memory_order_relaxed)));
            return;
        }
    }
    StopCustom();
}

float Music::GetClipLength() {
    if (!ourClip) return 0.0f;

    Il2CppClass* clipClass = Il2Cpp::FindClass("UnityEngine.AudioClip", Il2Cpp::GetImage("UnityEngine.AudioModule"));
    static const MethodInfo* getLength = nullptr;
    if (!getLength) getLength = clipClass ? Il2Cpp::GetMethod(clipClass, "get_length", 0) : nullptr;
    if (!getLength) return 0.0f;

    void* raw = Il2Cpp::UnboxRaw(Il2Cpp::Invoke(getLength, ourClip, nullptr));
    return raw ? *reinterpret_cast<float*>(raw) : 0.0f;
}

// Action dispatch

void Music::HandleAction(Action action, int index) {
    switch (action) {
    case Action::Play: {
        wantsCustom = true;
        playbackTime = 0.0f;

        // Remember the pick so it can start automatically once a blocking game
        // state (loading, results, menu, auction) has passed
        currentIndex.store(index, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(listMutex);
            if (index >= 0 && index < static_cast<int>(tracks.size())) {
                std::lock_guard<std::mutex> nameLock(nameMutex);
                currentName = tracks[index].name;
            }
        }

        if (!IsStateAllowed(CurrentMusicState())) {
            suspended = true;
            status.Set("Custom music is disabled in this game state");
            return;
        }

        suspended = false;
        musicPlayer = ResolveMusicPlayer();
        if (!musicPlayer) {
            status.Set("Game music player not found");
            return;
        }
        audioSource = ResolveSource();
        if (!audioSource) {
            status.Set("Music source not available");
            return;
        }
        StopPlayback();
        StartLoad(index);
        break;
    }
    case Action::Stop:
        StopCustom();
        break;
    case Action::Pause:
        if (playing.load(std::memory_order_relaxed) && !paused.load(std::memory_order_relaxed) && audioSource) {
            PauseSource(audioSource);
            paused.store(true, std::memory_order_release);
            status.Set("Paused");
        }
        break;
    case Action::Resume:
        if (playing.load(std::memory_order_relaxed) && paused.load(std::memory_order_relaxed) && audioSource) {
            PlaySource(audioSource);
            paused.store(false, std::memory_order_release);
            status.Set("Resumed");
        }
        break;
    case Action::Next:
    case Action::Prev: {
        if (suspended) {
            status.Set("Custom music is waiting for an available game state");
            return;
        }
        size_t count;
        {
            std::lock_guard<std::mutex> lock(listMutex);
            count = tracks.size();
        }
        if (count == 0) {
            status.Set("No tracks in the music folder");
            return;
        }
        musicPlayer = ResolveMusicPlayer();
        if (!musicPlayer) {
            status.Set("Game music player not found");
            return;
        }
        audioSource = ResolveSource();
        if (!audioSource) {
            status.Set("Music source not available");
            return;
        }

        const int current = currentIndex.load(std::memory_order_relaxed);
        const bool randomize = shuffle.load(std::memory_order_relaxed);
        const int next = (action == Action::Next)
            ? NextIndex(current, static_cast<int>(count), randomize)
            : PrevIndex(current, static_cast<int>(count), randomize);

        playbackTime = 0.0f;
        StopPlayback();
        StartLoad(next);
        break;
    }
    default:
        break;
    }
}

void Music::Pump() {
    Il2Cpp::ThreadAttach();

    const Action action = static_cast<Action>(pendingAction.exchange(static_cast<int>(Action::None), std::memory_order_acq_rel));
    if (action != Action::None) {
        const int index = pendingIndex.load(std::memory_order_relaxed);
        HandleAction(action, index);
    }

    UpdateStateGate();

    if (overrideActive) PollPhoneKeybinds();

    if (loading.load(std::memory_order_acquire)) {
        if (!webRequest) {
            loading.store(false, std::memory_order_release);
        } else {
            RequestApi& api = RequestApiFor();
            const bool done = api.isDone && Il2Cpp::UnboxBool(Il2Cpp::Invoke(api.isDone, webRequest, nullptr));
            if (!done) return;
            FinishLoad();
        }
    }

    if (playing.load(std::memory_order_acquire) && !paused.load(std::memory_order_relaxed)) {
        ApplyPlayback();
    }
}

void Music::Shutdown() {
    if (shutDown) return;
    shutDown = true;

    Il2Cpp::ThreadAttach();
    CancelLoad();
    ReleaseToGame();
}

// UI

void Music::RenderTab() {
    if (!Shared::BeginGameTab("Music")) return;

    status.Render();

    if (!listLoaded.load(std::memory_order_acquire)) Reload();

    ImGui::TextWrapped("Drop .wav, .mp3 or .ogg files into the \"music\" folder next to nrmm.dll.");
    if (ImGui::SmallButton("Reload folder")) Reload();
    ImGui::SameLine();

    std::vector<Track> snapshot;
    {
        std::lock_guard<std::mutex> lock(listMutex);
        snapshot = tracks;
    }
    ImGui::TextDisabled("%d track(s)", static_cast<int>(snapshot.size()));

    if (snapshot.empty()) {
        ImGui::EndTabItem();
        return;
    }

    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(snapshot.size())) selectedIndex = 0;

    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo("##track", snapshot[selectedIndex].name.c_str())) {
        for (int i = 0; i < static_cast<int>(snapshot.size()); ++i) {
            const bool selected = (i == selectedIndex);
            if (ImGui::Selectable(snapshot[i].name.c_str(), selected)) selectedIndex = i;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const bool isLoading = loading.load(std::memory_order_relaxed);
    const bool isPlaying = playing.load(std::memory_order_relaxed);
    const bool isPaused = paused.load(std::memory_order_relaxed);

    ImGui::BeginDisabled(isLoading);
    if (ImGui::Button("Play")) Request(Action::Play, selectedIndex);
    ImGui::SameLine();

    ImGui::BeginDisabled(!isPlaying);
    if (ImGui::Button("Stop")) Request(Action::Stop, -1);
    ImGui::SameLine();
    if (ImGui::Button(isPaused ? "Resume" : "Pause")) Request(isPaused ? Action::Resume : Action::Pause, -1);
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Prev")) Request(Action::Prev, -1);
    ImGui::SameLine();
    if (ImGui::Button("Next")) Request(Action::Next, -1);
    ImGui::EndDisabled();

    bool listMode = playlist.load(std::memory_order_relaxed);
    if (ImGui::Checkbox("Playlist", &listMode)) playlist.store(listMode, std::memory_order_relaxed);
    ImGui::SameLine();
    bool randomize = shuffle.load(std::memory_order_relaxed);
    if (ImGui::Checkbox("Shuffle", &randomize)) shuffle.store(randomize, std::memory_order_relaxed);

    if (isLoading) {
        ImGui::TextUnformatted("Loading...");
    } else if (isPlaying) {
        ImGui::Text("Now playing: %s%s", CurrentName().c_str(), isPaused ? " (paused)" : "");
    }

    ImGui::EndTabItem();
}
