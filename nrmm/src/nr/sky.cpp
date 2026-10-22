#include "pch.h"

#include "sky.h"
#include "nr/shared.h"
#include "globals.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>

// Path and text helpers for the SKY presets. Presets live in a "sky_presets"
// folder next to nrmm.dll, one plain "key=value" text file each

static std::wstring PresetFolderPath() {
    std::wstring directory = g_DllPath.empty() ? L"." : g_DllPath;
    if (!directory.empty() && directory.back() != L'\\' && directory.back() != L'/') directory += L'\\';
    return directory + L"sky_presets";
}

static std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), size);
    return out;
}

// Drops characters a file name cannot contain and trims the result, so a
// preset always lands directly inside the sky_presets folder
static std::string SanitizePresetName(const std::string& raw) {
    std::string name;
    for (char c : raw) {
        const unsigned char value = static_cast<unsigned char>(c);
        if (value < 32) continue;
        if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') continue;
        name += c;
    }

    const size_t first = name.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const size_t last = name.find_last_not_of(" \t");
    return name.substr(first, last - first + 1);
}

Sky::FloatOverride Sky::floatOverrides[] = {
    // Lighting
    {Section::Lighting, "Directional light", "DirectionalLightIntensity", 0.0f, 8.0f, false, 1.0f},
    {Section::Lighting, "Ambient intensity", "AmbientIntensity", 0.0f, 8.0f, false, 1.0f},
    {Section::Lighting, "Sky reflection", "SkyReflectionIntensity", 0.0f, 4.0f, false, 1.0f},

    // Scattering
    {Section::Scattering, "Molecular density", "MolecularDensity", 0.0f, 3.0f, false, 1.0f},
    {Section::Scattering, "Rayleigh multiplier", "RayleighMultiplier", 0.0f, 5.0f, false, 1.0f},
    {Section::Scattering, "Mie multiplier", "MieMultiplier", 0.0f, 5.0f, false, 1.0f},
    {Section::Scattering, "Scattering intensity", "ScatteringIntensity", 0.0f, 5.0f, false, 1.0f},
    {Section::Scattering, "Sky luminance", "SkyLuminance", 0.0f, 5.0f, false, 1.0f},
    {Section::Scattering, "Exposure", "Exposure", 0.0f, 8.0f, false, 1.0f},
    {Section::Scattering, "Precomputed sun", "PrecomputedSunIntensity", 0.0f, 5.0f, false, 1.0f},
    {Section::Scattering, "Precomputed moon", "PrecomputedMoonIntensity", 0.0f, 5.0f, false, 1.0f},

    // Fog
    {Section::Fog, "Fog scattering scale", "FogScatteringScale", 0.0f, 2.0f, false, 1.0f},
    {Section::Fog, "Global fog smooth", "GlobalFogSmooth", 0.0f, 1.0f, false, 0.5f},
    {Section::Fog, "Global fog density", "GlobalFogDensity", 0.0f, 5.0f, false, 1.0f},
    {Section::Fog, "Global fog distance", "GlobalFogDistance", 0.0f, 5000.0f, false, 1000.0f},
    {Section::Fog, "Height fog smooth", "HeightFogSmooth", 0.0f, 1.0f, false, 0.5f},
    {Section::Fog, "Height fog density", "HeightFogDensity", 0.0f, 5.0f, false, 1.0f},
    {Section::Fog, "Height fog distance", "HeightFogDistance", 0.0f, 5000.0f, false, 1000.0f},
    {Section::Fog, "Height fog start", "HeightFogStart", 0.0f, 200.0f, false, 0.0f},
    {Section::Fog, "Height fog end", "HeightFogEnd", 0.0f, 500.0f, false, 100.0f},

    // Clouds
    {Section::Clouds, "Static layer 1 speed", "StaticCloudLayer1Speed", -1.0f, 1.0f, false, 0.0f},
    {Section::Clouds, "Static layer 2 speed", "StaticCloudLayer2Speed", -1.0f, 1.0f, false, 0.0f},
    {Section::Clouds, "Static scattering", "StaticCloudScattering", 0.0f, 3.0f, false, 1.0f},
    {Section::Clouds, "Static extinction", "StaticCloudExtinction", 0.0f, 3.0f, false, 1.0f},
    {Section::Clouds, "Static saturation", "StaticCloudSaturation", 0.0f, 3.0f, false, 1.0f},
    {Section::Clouds, "Static opacity", "StaticCloudOpacity", 0.0f, 3.0f, false, 1.0f},
    {Section::Clouds, "Dynamic altitude", "DynamicCloudAltitude", 0.0f, 5.0f, false, 1.0f},
    {Section::Clouds, "Dynamic direction", "DynamicCloudDirection", 0.0f, 1.0f, false, 0.5f},
    {Section::Clouds, "Dynamic speed", "DynamicCloudSpeed", 0.0f, 1.0f, false, 0.1f},
    {Section::Clouds, "Dynamic density", "DynamicCloudDensity", 0.0f, 5.0f, false, 1.0f},

    // Outer space
    {Section::OuterSpace, "Sun texture size", "SunTextureSize", 0.0f, 2.0f, false, 1.0f},
    {Section::OuterSpace, "Sun texture intensity", "SunTextureIntensity", 0.0f, 5.0f, false, 1.0f},
    {Section::OuterSpace, "Moon texture size", "MoonTextureSize", 0.0f, 2.0f, false, 1.0f},
    {Section::OuterSpace, "Moon texture intensity", "MoonTextureIntensity", 0.0f, 5.0f, false, 1.0f},
    {Section::OuterSpace, "Star scintillation", "RegularStarsScintillation", 0.0f, 1.0f, false, 0.5f},
    {Section::OuterSpace, "Star intensity", "RegularStarsIntensity", 0.0f, 3.0f, false, 1.0f},
    {Section::OuterSpace, "Milky way", "MilkyWayIntensity", 0.0f, 3.0f, false, 1.0f},

    // Weather
    {Section::Weather, "Rain light intensity", "RainLightIntensity", 0.0f, 1.0f, false, 0.0f},
    {Section::Weather, "Rain medium intensity", "RainMediumIntensity", 0.0f, 1.0f, false, 0.0f},
    {Section::Weather, "Rain heavy intensity", "RainHeavyIntensity", 0.0f, 1.0f, false, 0.0f},
    {Section::Weather, "Snow intensity", "SnowIntensity", 0.0f, 1.0f, false, 0.0f},
    {Section::Weather, "Rain light volume", "RainLightVolume", 0.0f, 1.0f, false, 0.0f},
    {Section::Weather, "Rain medium volume", "RainMediumVolume", 0.0f, 1.0f, false, 0.0f},
    {Section::Weather, "Rain heavy volume", "RainHeavyVolume", 0.0f, 1.0f, false, 0.0f},
    {Section::Weather, "Wind light volume", "WindLightVolume", 0.0f, 1.0f, false, 0.0f},
    {Section::Weather, "Wind medium volume", "WindMediumVolume", 0.0f, 1.0f, false, 0.0f},
    {Section::Weather, "Wind heavy volume", "WindHeavyVolume", 0.0f, 1.0f, false, 0.0f},
    {Section::Weather, "Wind speed", "WindSpeed", 0.0f, 1.0f, false, 0.0f},
    {Section::Weather, "Wind direction", "WindDirection", 0.0f, 1.0f, false, 0.5f},
    {Section::Weather, "Thunder", "ThunderMultiplier", 0.0f, 3.0f, false, 0.0f},
};

Sky::ColorOverride Sky::colorOverrides[] = {
    {Section::Lighting, "Directional light", "DirectionalLightColor", false, 1.0f, 1.0f, 1.0f, 1.0f},
    {Section::Lighting, "Ambient", "AmbientColor", false, 1.0f, 1.0f, 1.0f, 1.0f},
    {Section::Lighting, "Ambient sky", "AmbientSkyColor", false, 1.0f, 1.0f, 1.0f, 1.0f},
    {Section::Lighting, "Ambient equator", "AmbientEquatorColor", false, 1.0f, 1.0f, 1.0f, 1.0f},
    {Section::Lighting, "Ambient ground", "AmbientGroundColor", false, 1.0f, 1.0f, 1.0f, 1.0f},

    {Section::Scattering, "Rayleigh", "RayleighColor", false, 1.0f, 1.0f, 1.0f, 1.0f},
    {Section::Scattering, "Mie", "MieColor", false, 1.0f, 1.0f, 1.0f, 1.0f},
    {Section::Scattering, "Transmittance", "TransmittanceColor", false, 1.0f, 1.0f, 1.0f, 1.0f},

    {Section::Clouds, "Static cloud", "StaticCloudColor", false, 1.0f, 1.0f, 1.0f, 1.0f},
    {Section::Clouds, "Dynamic cloud 1", "DynamicCloudColor1", false, 1.0f, 1.0f, 1.0f, 1.0f},
    {Section::Clouds, "Dynamic cloud 2", "DynamicCloudColor2", false, 1.0f, 1.0f, 1.0f, 1.0f},

    {Section::OuterSpace, "Sun texture", "SunTextureColor", false, 1.0f, 1.0f, 1.0f, 1.0f},
    {Section::OuterSpace, "Moon texture", "MoonTextureColor", false, 1.0f, 1.0f, 1.0f, 1.0f},

    // Rain is darkened by default, as requested
    {Section::Weather, "Rain color", "RainColor", true, 0.2f, 0.2f, 0.2f, 1.0f},
    {Section::Weather, "Snow color", "SnowColor", false, 1.0f, 1.0f, 1.0f, 1.0f},
};

void Sky::CaptureTime(Il2CppObject* god) {
    SavedTime state;
    Il2Cpp::GetInstanceFieldValue(god, "skyLerp", state.skyLerp);
    Il2Cpp::GetInstanceFieldValue(god, "daySkyLerp", state.daySkyLerp);
    Il2Cpp::GetInstanceFieldValue(god, "azureSkyLerp", state.azureSkyLerp);
    Il2Cpp::GetInstanceFieldValue(god, "timeHour", state.timeHour);
    Il2Cpp::GetInstanceFieldValue(god, "timeHourTarget", state.timeHourTarget);
    Il2Cpp::GetInstanceFieldValue(god, "timeMinutes", state.timeMinutes);
    Il2Cpp::GetInstanceFieldValue(god, "timeDayCount", state.timeDayCount);
    Il2Cpp::GetInstanceFieldValue(god, "timePauseClock", state.pauseClock);
    savedTime = state;
}

void Sky::RestoreTime(Il2CppObject* god) {
    const SavedTime& state = savedTime;
    Il2Cpp::SetInstanceFieldValue(god, "skyLerp", state.skyLerp);
    Il2Cpp::SetInstanceFieldValue(god, "daySkyLerp", state.daySkyLerp);
    Il2Cpp::SetInstanceFieldValue(god, "azureSkyLerp", state.azureSkyLerp);
    Il2Cpp::SetInstanceFieldValue(god, "timeHour", state.timeHour);
    Il2Cpp::SetInstanceFieldValue(god, "timeHourTarget", state.timeHourTarget);
    Il2Cpp::SetInstanceFieldValue(god, "timeMinutes", state.timeMinutes);
    Il2Cpp::SetInstanceFieldValue(god, "timeDayCount", state.timeDayCount);
    Il2Cpp::SetInstanceFieldValue(god, "timePauseClock", state.pauseClock);
}

void Sky::ApplyTime(Il2CppObject* god) {
    if (!god) return;

    const bool overriding = timeOverride.load(std::memory_order_relaxed);
    const bool freeze = timeFreeze.load(std::memory_order_relaxed);
    const bool wasCaptured = timeCaptured.load(std::memory_order_relaxed);

    // Snapshot the game's clock the first frame the override turns on and hand
    // it back once it turns off, so the override is temporary rather than a
    // one-way change
    if (overriding && !wasCaptured) {
        CaptureTime(god);
        timeCaptured.store(true, std::memory_order_relaxed);
    } else if (!overriding && wasCaptured) {
        RestoreTime(god);
        timeCaptured.store(false, std::memory_order_relaxed);
    }

    // The full-day override bypasses the game's skyLerp mapping, which only
    // covers the night window, so the clock is held while it is active
    const bool pause = overriding || freeze;
    bool pauseClock = false;
    Il2Cpp::GetInstanceFieldValue(god, "timePauseClock", pauseClock);
    if (pauseClock != pause) Il2Cpp::SetInstanceFieldValue(god, "timePauseClock", pause);

    if (!overriding) return;

    float hour = timeValue.load(std::memory_order_relaxed);
    if (hour < 0.0f) hour = 0.0f;
    if (hour > kMaxHour) hour = kMaxHour;

    float start = 0.0f;
    float end = 0.0f;
    Il2Cpp::GetInstanceFieldValue(god, "azureSkyStart", start);
    Il2Cpp::GetInstanceFieldValue(god, "azureSkyEnd", end);
    float span = end - start;
    if (span <= 0.0f) span += 24.0f;

    // skyLerp/daySkyLerp drive the game's own night curves; map the hour onto
    // the night window so they stay consistent with the clock
    float withinNight = fmodf(hour - start + 24.0f, 24.0f);
    float dayLerp = withinNight / span;
    if (dayLerp > 1.0f) dayLerp = 1.0f;

    const float hourFloor = floorf(hour);
    const float fraction = hour - hourFloor;

    Il2Cpp::SetInstanceFieldValue(god, "azureSkyLerp", hour);
    Il2Cpp::SetInstanceFieldValue(god, "daySkyLerp", dayLerp);
    Il2Cpp::SetInstanceFieldValue(god, "skyLerp", dayLerp * 12.0f);
    Il2Cpp::SetInstanceFieldValue(god, "timeHour", hourFloor);
    Il2Cpp::SetInstanceFieldValue(god, "timeHourTarget", fraction);
}

void Sky::SnapshotTime(Il2CppObject* god) {
    float hour = 0.0f;
    Il2Cpp::GetInstanceFieldValue(god, "timeHour", hour);
    float minutes = 0.0f;
    Il2Cpp::GetInstanceFieldValue(god, "timeMinutes", minutes);
    curHour.store(hour + minutes / 60.0f, std::memory_order_relaxed);
}

void Sky::ApplySettingOverrides(Il2CppObject* settings) {
    if (!settings) return;

    bool any = false;
    for (const FloatOverride& entry : floatOverrides) if (entry.enabled.load(std::memory_order_relaxed)) any = true;
    for (const ColorOverride& entry : colorOverrides) if (entry.enabled.load(std::memory_order_relaxed)) any = true;
    if (!any) return;

    for (const FloatOverride& entry : floatOverrides) {
        if (!entry.enabled.load(std::memory_order_relaxed)) continue;
        const float value = entry.value.load(std::memory_order_relaxed);
        Il2Cpp::SetInstanceFieldValue(settings, entry.field, value);
    }

    for (const ColorOverride& entry : colorOverrides) {
        if (!entry.enabled.load(std::memory_order_relaxed)) continue;
        const Shared::Colour color{entry.r.load(std::memory_order_relaxed), entry.g.load(std::memory_order_relaxed), entry.b.load(std::memory_order_relaxed), entry.a.load(std::memory_order_relaxed)};
        Il2Cpp::SetInstanceFieldValue(settings, entry.field, color);
    }
}

void Sky::RenderFloatOverrides(const char* title, Section section) {
    bool any = false;
    for (const FloatOverride& entry : floatOverrides) if (entry.section == section) any = true;
    if (!any) return;

    if (!ImGui::CollapsingHeader(title)) return;

    for (FloatOverride& entry : floatOverrides) {
        if (entry.section != section) continue;
        bool enabled = entry.enabled.load(std::memory_order_relaxed);
        float value = entry.value.load(std::memory_order_relaxed);

        ImGui::PushID(entry.field);
        if (ImGui::Checkbox("##enable", &enabled)) entry.enabled.store(enabled, std::memory_order_relaxed);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(190.0f);
        ImGui::BeginDisabled(!enabled);
        if (ImGui::SliderFloat(entry.label, &value, entry.min, entry.max, "%.3f")) entry.value.store(value, std::memory_order_relaxed);
        ImGui::EndDisabled();
        ImGui::PopID();
    }
}

void Sky::RenderColorOverrides(const char* title, Section section) {
    bool any = false;
    for (const ColorOverride& entry : colorOverrides) if (entry.section == section) any = true;
    if (!any) return;

    if (!ImGui::CollapsingHeader(title)) return;

    for (ColorOverride& entry : colorOverrides) {
        if (entry.section != section) continue;
        bool enabled = entry.enabled.load(std::memory_order_relaxed);
        float color[4] = {
            entry.r.load(std::memory_order_relaxed),
            entry.g.load(std::memory_order_relaxed),
            entry.b.load(std::memory_order_relaxed),
            entry.a.load(std::memory_order_relaxed),
        };

        ImGui::PushID(entry.field);
        if (ImGui::Checkbox("##enable", &enabled)) entry.enabled.store(enabled, std::memory_order_relaxed);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(190.0f);
        ImGui::BeginDisabled(!enabled);
        if (ImGui::ColorEdit4(entry.label, color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar)) {
            entry.r.store(color[0], std::memory_order_relaxed);
            entry.g.store(color[1], std::memory_order_relaxed);
            entry.b.store(color[2], std::memory_order_relaxed);
            entry.a.store(color[3], std::memory_order_relaxed);
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }
}

void Sky::RenderTimeSection() {
    if (!ImGui::CollapsingHeader("Time", ImGuiTreeNodeFlags_DefaultOpen)) return;

    const float gameHour = curHour.load(std::memory_order_relaxed);
    const int gameMinutes = static_cast<int>(lroundf(gameHour * 60.0f));
    ImGui::TextDisabled("Game clock: %02d:%02d", (gameMinutes / 60) % 24, gameMinutes % 60);

    bool override = timeOverride.load(std::memory_order_relaxed);
    if (ImGui::Checkbox("Override time", &override)) {
        timeOverride.store(override, std::memory_order_relaxed);
        if (override) timeValue.store(gameHour, std::memory_order_relaxed);
    }
    ImGui::SetItemTooltip("Holds the clock and drives the sky at the selected time");

    float value = timeValue.load(std::memory_order_relaxed);
    ImGui::BeginDisabled(!override);
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::SliderFloat("Time of day", &value, 0.0f, kMaxHour, "%.3f")) {
        // Snap to the minute
        value = roundf(value * 60.0f) / 60.0f;
        if (value < 0.0f) value = 0.0f;
        if (value > kMaxHour) value = kMaxHour;
        timeValue.store(value, std::memory_order_relaxed);
    }
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("00:00 to 23:59");
    ImGui::SameLine();

    const int valueMinutes = static_cast<int>(lroundf(value * 60.0f));
    ImGui::Text("%02d:%02d", (valueMinutes / 60) % 24, valueMinutes % 60);

    bool freeze = timeFreeze.load(std::memory_order_relaxed);
    if (ImGui::Checkbox("Freeze time", &freeze)) timeFreeze.store(freeze, std::memory_order_relaxed);
}

void Sky::ReloadPresets() {
    std::vector<Shared::MediaFile> found = Shared::ScanMediaFolder(L"sky_presets", { L".txt" });
    {
        std::lock_guard<std::mutex> lock(presetMutex);
        presets = std::move(found);
    }
    presetsLoaded.store(true, std::memory_order_release);
}

void Sky::SavePreset(const std::string& rawName) {
    const std::string name = SanitizePresetName(rawName);
    if (name.empty()) {
        presetStatus.Set("Enter a preset name");
        return;
    }

    const std::wstring path = PresetFolderPath() + L"\\" + Utf8ToWide(name) + L".txt";
    std::ofstream file(path.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        presetStatus.Set("Could not write preset \"" + name + "\"");
        return;
    }

    file << "# nrmm sky preset\n";
    file << "time_override=" << (timeOverride.load(std::memory_order_relaxed) ? 1 : 0) << '\n';
    file << "time_freeze=" << (timeFreeze.load(std::memory_order_relaxed) ? 1 : 0) << '\n';
    file << "time_value=" << timeValue.load(std::memory_order_relaxed) << '\n';

    for (const FloatOverride& entry : floatOverrides) {
        file << "float." << entry.field << ".enabled=" << (entry.enabled.load(std::memory_order_relaxed) ? 1 : 0) << '\n';
        file << "float." << entry.field << ".value=" << entry.value.load(std::memory_order_relaxed) << '\n';
    }
    for (const ColorOverride& entry : colorOverrides) {
        file << "color." << entry.field << ".enabled=" << (entry.enabled.load(std::memory_order_relaxed) ? 1 : 0) << '\n';
        file << "color." << entry.field << ".r=" << entry.r.load(std::memory_order_relaxed) << '\n';
        file << "color." << entry.field << ".g=" << entry.g.load(std::memory_order_relaxed) << '\n';
        file << "color." << entry.field << ".b=" << entry.b.load(std::memory_order_relaxed) << '\n';
        file << "color." << entry.field << ".a=" << entry.a.load(std::memory_order_relaxed) << '\n';
    }
    file.flush();

    presetStatus.Set("Saved preset \"" + name + "\"");

    // Rescan and reselect the preset that was just written
    ReloadPresets();
    {
        std::lock_guard<std::mutex> lock(presetMutex);
        for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
            if (presets[i].name == name) {
                selectedPreset = i;
                break;
            }
        }
    }
}

void Sky::LoadPreset(const std::string& path) {
    std::ifstream file(Utf8ToWide(path).c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        presetStatus.Set("Could not open the preset");
        return;
    }

    std::unordered_map<std::string, std::string> values;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t separator = line.find('=');
        if (separator == std::string::npos) continue;
        values[line.substr(0, separator)] = line.substr(separator + 1);
    }

    auto number = [&values](const std::string& key, float fallback) {
        const auto it = values.find(key);
        if (it == values.end()) return fallback;
        return std::strtof(it->second.c_str(), nullptr);
    };
    auto flag = [&values](const std::string& key, bool fallback) {
        const auto it = values.find(key);
        if (it == values.end()) return fallback;
        return it->second == "1" || it->second == "true";
    };

    // Only overwrite entries present in the file, so presets saved by an older
    // build still load with the new entries left untouched
    for (FloatOverride& entry : floatOverrides) {
        const std::string prefix = std::string("float.") + entry.field;
        if (values.contains(prefix + ".enabled")) entry.enabled.store(flag(prefix + ".enabled", false), std::memory_order_relaxed);
        if (values.contains(prefix + ".value")) entry.value.store(number(prefix + ".value", entry.value.load(std::memory_order_relaxed)), std::memory_order_relaxed);
    }
    for (ColorOverride& entry : colorOverrides) {
        const std::string prefix = std::string("color.") + entry.field;
        if (values.contains(prefix + ".enabled")) entry.enabled.store(flag(prefix + ".enabled", false), std::memory_order_relaxed);
        entry.r.store(number(prefix + ".r", entry.r.load(std::memory_order_relaxed)), std::memory_order_relaxed);
        entry.g.store(number(prefix + ".g", entry.g.load(std::memory_order_relaxed)), std::memory_order_relaxed);
        entry.b.store(number(prefix + ".b", entry.b.load(std::memory_order_relaxed)), std::memory_order_relaxed);
        entry.a.store(number(prefix + ".a", entry.a.load(std::memory_order_relaxed)), std::memory_order_relaxed);
    }

    if (values.contains("time_override")) timeOverride.store(flag("time_override", false), std::memory_order_relaxed);
    if (values.contains("time_freeze")) timeFreeze.store(flag("time_freeze", false), std::memory_order_relaxed);
    if (values.contains("time_value")) timeValue.store(number("time_value", timeValue.load(std::memory_order_relaxed)), std::memory_order_relaxed);

    const std::string fileName = Shared::FileName(path);
    presetStatus.Set("Loaded preset \"" + fileName.substr(0, fileName.find_last_of('.')) + "\"");
}

void Sky::DeletePreset(const std::string& path) {
    std::error_code error;
    std::filesystem::remove(Utf8ToWide(path), error);
    if (error) {
        presetStatus.Set("Could not delete the preset");
        return;
    }

    presetStatus.Set("Deleted preset");
    ReloadPresets();
}

void Sky::RenderPresetsSection() {
    if (!ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen)) return;

    if (!presetsLoaded.load(std::memory_order_acquire)) ReloadPresets();

    presetStatus.Render();

    if (ImGui::SmallButton("Refresh")) ReloadPresets();
    ImGui::SameLine();

    std::vector<Shared::MediaFile> snapshot;
    {
        std::lock_guard<std::mutex> lock(presetMutex);
        snapshot = presets;
    }
    ImGui::TextDisabled("%d preset(s)", static_cast<int>(snapshot.size()));

    if (selectedPreset >= static_cast<int>(snapshot.size())) selectedPreset = snapshot.empty() ? 0 : static_cast<int>(snapshot.size()) - 1;

    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo("##preset", snapshot.empty() ? "(none)" : snapshot[selectedPreset].name.c_str())) {
        for (int i = 0; i < static_cast<int>(snapshot.size()); ++i) {
            const bool selected = (i == selectedPreset);
            if (ImGui::Selectable(snapshot[i].name.c_str(), selected)) selectedPreset = i;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::BeginDisabled(snapshot.empty());
    if (ImGui::Button("Load")) LoadPreset(snapshot[selectedPreset].path);
    ImGui::SameLine();
    if (ImGui::Button("Delete")) ImGui::OpenPopup("Delete preset?");
    ImGui::EndDisabled();

    if (ImGui::BeginPopupModal("Delete preset?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete \"%s\"?", snapshot.empty() ? "" : snapshot[selectedPreset].name.c_str());
        if (ImGui::Button("Delete", ImVec2(120.0f, 0.0f))) {
            if (!snapshot.empty()) DeletePreset(snapshot[selectedPreset].path);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::TextUnformatted("Save current settings as:");
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("##presetname", "preset name", presetName, sizeof(presetName));
    ImGui::SameLine();
    if (ImGui::Button("Save")) SavePreset(presetName);
}

void Sky::Pump() {
    Il2Cpp::ThreadAttach();

    Il2CppObject* god = Shared::God();
    if (!god) return;

    ApplyTime(god);
    SnapshotTime(god);
}

void Sky::ApplySettings(Il2CppObject* settings) {
    ApplySettingOverrides(settings);
}

void Sky::RenderTab() {
    if (!Shared::BeginGameTab("Sky")) return;

    RenderPresetsSection();
    RenderTimeSection();

    RenderFloatOverrides("Lighting", Section::Lighting);
    RenderColorOverrides("Lighting colours", Section::Lighting);
    RenderFloatOverrides("Scattering", Section::Scattering);
    RenderColorOverrides("Scattering colours", Section::Scattering);
    RenderFloatOverrides("Fog", Section::Fog);
    RenderFloatOverrides("Clouds", Section::Clouds);
    RenderColorOverrides("Cloud colours", Section::Clouds);
    RenderFloatOverrides("Outer space", Section::OuterSpace);
    RenderColorOverrides("Outer space colours", Section::OuterSpace);
    RenderFloatOverrides("Rain / snow / wind", Section::Weather);
    RenderColorOverrides("Weather colours", Section::Weather);
}
