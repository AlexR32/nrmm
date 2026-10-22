#pragma once
#include "pch.h"
#include "il2cpp/il2cpp.h"
#include "nr/shared.h"
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

// Sky tab: a live editor for the Azure Sky Manager settings (lighting,
// scattering, fog, clouds, outer space and weather) plus a game-time override
// and freeze.
//
// The overrides are written into the freshly computed AzureSkySettings through a
// detour on AzureProfileController.GetProfileSettings, before the game applies
// them to the sky/fog shaders. That leaves the game's own day/night animation
// intact and is fully reversible: the settings are rebuilt from the profiles
// every frame, so clearing an override restores the game's value.
//
// Everything that touches managed objects runs on the game's script thread:
//   - Pump()          from the GodConstant.Update hook (time)
//   - ApplySettings() from the AzureProfileController.GetProfileSettings hook
// The render thread only reads atomic snapshots.

class Sky {
public:
    static void RenderTab();

    // GodConstant.Update hook (script thread): applies the time override/freeze
    // and snapshots the clock the widgets display
    static void Pump();

    // AzureProfileController.GetProfileSettings hook (script thread): rewrites
    // the freshly computed settings with the user's overrides
    static void ApplySettings(Il2CppObject* settings);

private:
    // The flattened values Azure computes each frame and pushes into the sky/fog
    // shaders. Editing these (instead of the profile curves) leaves the game's
    // own day/night animation intact and is fully reversible: the settings are
    // rebuilt from the profiles every frame, so simply clearing an override
    // restores the game's value.
    enum class Section { Lighting, Scattering, Fog, Clouds, OuterSpace, Weather };

    struct FloatOverride {
        Section section;
        const char* label;
        const char* field;
        float min;
        float max;
        std::atomic_bool enabled;
        std::atomic<float> value;
    };

    struct ColorOverride {
        Section section;
        const char* label;
        const char* field;
        std::atomic_bool enabled;
        std::atomic<float> r;
        std::atomic<float> g;
        std::atomic<float> b;
        std::atomic<float> a;
    };

    static FloatOverride floatOverrides[];
    static ColorOverride colorOverrides[];

    // Time control. The game's own clock is snapshotted on the first frame the
    // override turns on and handed back when it turns off, so the override is
    // temporary rather than a one-way change
    struct SavedTime {
        float skyLerp = 0.0f;
        float daySkyLerp = 0.0f;
        float azureSkyLerp = 0.0f;
        float timeHour = 20.0f;
        float timeHourTarget = 0.0f;
        float timeMinutes = 0.0f;
        int timeDayCount = 0;
        bool pauseClock = false;
    };

    static void ApplyTime(Il2CppObject* god);
    static void SnapshotTime(Il2CppObject* god);
    static void CaptureTime(Il2CppObject* god);
    static void RestoreTime(Il2CppObject* god);

    // Overrides
    static void ApplySettingOverrides(Il2CppObject* settings);

    // UI
    static void RenderTimeSection();
    static void RenderPresetsSection();
    static void RenderFloatOverrides(const char* title, Section section);
    static void RenderColorOverrides(const char* title, Section section);

    // Presets: every override plus the time state is written to one text file
    // per preset in a "sky_presets" folder next to nrmm.dll. All of this is
    // plain filesystem work driven from the render thread, so it carries no
    // thread-safety concerns beyond the list mutex
    static void ReloadPresets();
    static void SavePreset(const std::string& name);
    static void LoadPreset(const std::string& path);
    static void DeletePreset(const std::string& path);

    static inline std::atomic_bool timeOverride{false};
    static inline std::atomic_bool timeFreeze{false};
    static inline std::atomic<float> timeValue{0.0f};
    static inline std::atomic<float> curHour{20.0f};

    static inline SavedTime savedTime{};
    static inline std::atomic_bool timeCaptured{false};

    // Preset list, published by the render thread under presetMutex
    inline static Shared::Status presetStatus{"Sky"};
    inline static std::mutex presetMutex;
    inline static std::vector<Shared::MediaFile> presets;
    inline static std::atomic_bool presetsLoaded{false};
    inline static int selectedPreset = 0;
    inline static char presetName[64] = "";

    // 23:59, the last minute of the day the slider can reach
    static constexpr float kMaxHour = 23.0f + 59.0f / 60.0f;
};
