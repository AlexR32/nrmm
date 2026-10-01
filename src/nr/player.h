#pragma once
#include "pch.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"
#include "nr/enums.h"
#include "nr/shared.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

// Player tab: the player car wheel/induction/fuel type, ending the night, money
// and reputation grants, the parts unlock and the traffic toggle. The widgets
// read a small snapshot that RefreshSnapshot() rebuilds on the script thread
// from the GodConstant.Update hook, so the render thread never touches managed
// objects for display

class Player {
public:
    static void RenderTab();

    // Called from the GodConstant.Update hook, on the script thread
    static void RefreshSnapshot();

    // We need these in world tab
    static std::string MeetspotName(int32_t id);
    static Il2CppObject* LoadMeetspotData(int32_t id);

private:
    // Written by RefreshSnapshot() on the script thread, read by the render
    // thread. Each field is atomic so the widgets never race the writer
    struct Snapshot {
        std::atomic<int> raceCrewTypeIndex{-1};

        std::atomic_bool trafficAvailable{ false };
        std::atomic_bool trafficEnabled{ true };
    };

    static void ApplyRaceCrewType(int index);

    // Meetspot owner editor. LoadMeetspotData/SaveMeetspotData are the game's
    // own ES3-backed accessors
    static void SelectOwnerMeetspot(int index);
    static void SaveOwnerMeetspot();

    static bool SaveMeetspotData(int32_t id, Il2CppObject* data);

    static void EndNight();
    static void UnlockAllParts();
    //static void AddMoneyRep();

    static Shared::Status status;
    static Snapshot snapshot;

    // Meetspot owner editor. The selection and edit fields belong to the render
    // thread; the loaded* values are published by the script thread after a
    // LoadMeetspotData call and copied into the edit fields on a version bump
    static int ownerMeetSpotIndex;
    static int ownerCrewIndex;
    static float ownerStrength;
    static std::atomic<int> loadedDataVersion;
    static std::atomic_bool loadedDataValid;
    static std::atomic<int> loadedOwnerIndex;
    static std::atomic<float> loadedStrength;
    //static std::atomic<int> loadedLastUpdatedDay;
    //static std::atomic<float> loadedLastDaySkyLerp;
    static int seenDataVersion;

    //static int moneyToAdd;
    //static float repToAdd;
    //static int meetSpotIndex;
    //static float nightRepToAdd;
    //static int debtToAdd;
    //static int bettingMoneyToAdd;

    // Game constants
    static constexpr int kUnlockGameRep = 34275;
};
