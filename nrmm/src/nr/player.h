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
// and reputation grants, the parts unlock and the traffic toggle.

class Player {
public:
    static void RenderTab();
    static void RefreshSnapshot();

    // We need these in world tab
    static std::string MeetspotName(int32_t id);
    static Il2CppObject* LoadMeetspotData(int32_t id);

    // Crew restrictions. Written by the checkbox in the "Meetspot data"
    // section and read on the script thread by the Hooks detours for
    // doesCarPassRestriction and DoesPlayerMeetCrewSpec; while set the player's
    // car always passes every crew requirement
    inline static std::atomic_bool disableCrewRestrictions{false};

private:
    // Written by RefreshSnapshot() on the script thread, read by the render
    // thread. Each field is atomic so the widgets never race the writer
    struct Snapshot {
        std::atomic<int> raceCrewTypeIndex{-1};

        // player_NightRep, surfaced like the current car stats so it can be
        // edited directly and held. changeMoneyRep clamps it to 3.0, so this
        // path writes the field itself instead
        std::atomic<float> nightRep{0.0f};
    };

    static void ApplyRaceCrewType(int index);

    // Meetspot owner editor. LoadMeetspotData/SaveMeetspotData are the game's
    // own ES3-backed accessors
    static void SelectOwnerMeetspot(int index);
    static void SaveOwnerMeetspot();

    static bool SaveMeetspotData(int32_t id, Il2CppObject* data);

    static void EndNight();
    static void UnlockAllParts();

    // Flushes the game's ES3 cache to disk so menu edits persist immediately
    static void ForceSaveGame();

    // Money / reputation. Each category drives the game's own
    // GodConstant.changeMoneyRep, which clamps the value and persists it through
    // ES3. The arguments a category does not own are zeroed so the categories
    // stay independent (meetspot rep is addressed by meetSpotId; id 0 would
    // instead target crew rep, so meetspot rep always passes a real spot)
    static void ChangeMoneyRep(int money, float rep, int32_t meetSpotId, float nightRep, int debt, int betting, const std::string& message);
    static void AddMoney();
    static void AddMeetspotRep();
    static void AddDebt();
    static void RemoveDebt();
    static void AddBettingMoney();
    static void RemoveBettingMoney();

    inline static Shared::Status status{"Player"};
    inline static Snapshot snapshot;

    // Meetspot owner editor. The selection and edit fields belong to the render
    // thread; the loaded* values are published by the script thread after a
    // LoadMeetspotData call and copied into the edit fields on a version bump
    inline static int ownerMeetSpotIndex = 0;
    inline static int ownerCrewIndex = 0;
    inline static float ownerStrength = 0.0f;
    inline static std::atomic<int> loadedDataVersion{0};
    inline static std::atomic_bool loadedDataValid{false};
    inline static std::atomic<int> loadedOwnerIndex{-1};
    inline static std::atomic<float> loadedStrength{0.0f};
    //static std::atomic<int> loadedLastUpdatedDay;
    //static std::atomic<float> loadedLastDaySkyLerp;
    inline static int seenDataVersion = 0;

    // Night reputation freeze, mirroring the current car stats: while held the
    // menu value is rewritten each frame, otherwise the live value is tracked
    inline static std::atomic_bool freezeNightRep{false};
    inline static std::atomic<float> frozenNightRep{0.0f};

    // Money / reputation edit fields. Owned by the render thread; read when an
    // action snapshots them and posts the work to the script thread
    inline static int moneyToAdd = 0;
    inline static float repToAdd = 0.0f;
    inline static int meetSpotIndex = 0;
    inline static int debtToAdd = 0;
    inline static int bettingMoneyToAdd = 0;

    // Game constants
    static constexpr int kUnlockGameRep = 34275;
};
