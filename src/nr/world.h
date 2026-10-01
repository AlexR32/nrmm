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

class World {
public:
    static void RenderTab();

    // Called from the GodConstant.Update hook, on the script thread
    static void RefreshSnapshot();

private:
    // Written by RefreshSnapshot() on the script thread, read by the render
    // thread. Each field is atomic so the widgets never race the writer
    struct Snapshot {
        std::atomic_bool trafficAvailable{false};
        std::atomic_bool trafficEnabled{true};

        // True while GodConstant.floatingPoint_drivingScene is set. The travel
        // and meetspot attack lists are only valid inside a driving scene
        std::atomic_bool sceneAvailable{false};
    };

    // Meetspot attack.
    // A local_meetspots entry that the player's crew owns, identified by its
    // GodConstant.MeetSpots_All id (changeScene.targetMeetSpot)
    struct MeetspotTarget {
        int32_t id;
        std::string name;
    };

    // Rebuilds attackTargets on the script thread: walks
    // floatingPoint_drivingScene.local_meetspots and keeps the entries whose
    // MeetspotData.owner matches GodConstant.player_raceCrew
    static void RequestAttackTargets();
    static void ForceMeetspotAttack();

    static void ToggleTraffic(bool active);

    // Fast travel. FastTravelSystem.instance_.trucks_ is scanned on the script
    // thread and the truck display names are published to the render thread
    static void RequestTravelTargets();
    static void Travel();

    // Garages. drivingScene.homegarageSpot_garages is scanned on the script
    // thread; each entry is marked owned when GodConstant.owned_homeGarages
    // contains its homegarageSpotID
    struct GarageTarget {
        int32_t id;
        std::string name;
        bool owned;
    };

    static void RequestGarageTargets();
    static void GoToGarage(int index);
    static void BuyGarage(int index);
    static void AddGarageToOwned(int index);
    static Il2CppObject* ResolveGarageScene(Il2CppObject* god, int index);
    static bool SaveOwnedGarages(Il2CppObject* god, Il2CppObject* ownedGarages);

    static Shared::Status status;
    static Snapshot snapshot;

    // Attack targets live on the script thread; the render thread only ever
    // reads them under the mutex
    static std::vector<MeetspotTarget> attackTargets;
    static std::mutex attackTargetsMutex;
    static std::atomic_bool attackTargetsRequested;
    static int attackTargetIndex;
    static int32_t attackTargetId;

    // Fast travel truck names, rebuilt on the script thread. The render thread
    // only reads the copy under the mutex
    static std::vector<std::string> travelTargets;
    static std::mutex travelMutex;
    static std::atomic_bool travelRequested;
    static int travelIndex;

    // Garages, rebuilt on the script thread like the lists above
    static std::vector<GarageTarget> garageTargets;
    static std::mutex garageMutex;
    static std::atomic_bool garageRequested;
    static int garageIndex;

    // Auto-refresh state; only ever touched from the script thread
    static bool wasSceneLoaded;

};
