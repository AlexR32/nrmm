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

// World tab: traffic, meetspot attacks, truck fast travel and destination
// travel. The meetspot/travel/destination lists are rebuilt on the script
// thread, and the widget state is read from atomics or copies taken under a
// mutex, so the render thread never touches managed objects for display

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

    // Fast travel. The list is rebuilt on the script thread from the fast
    // travel stations plus every garage, car storage, meetspot and gas station,
    // and the names are published to the render thread. Travel() teleports the
    // player car to the target transform itself, reproducing what
    // drivingScene.fastTravel_Basic does to the car without its cost or
    // cinematic
    enum class TravelKind {
        Truck,
        Garage,
        CarStorage,
        Meetspot,
        GasStation,
    };

    struct TravelTarget {
        TravelKind kind;
        int sourceIndex; // index within the source array, -1 when there is none
        std::string name;
        // Gas stations are collected by component rather than by a stable list,
        // so the changeScene is kept to resolve the transform later
        Il2CppObject* changeScene = nullptr;
        // Meetspots and gas stations only define a position, so they keep the
        // car's current heading instead of adopting the target's rotation
        bool applyRotation = true;
    };

    static void RequestTravelTargets();
    static void Travel();
    static Il2CppObject* ResolveTravelTransform(Il2CppObject* scene, const TravelTarget& target);
    static Il2CppObject* ResolveChangeSceneSpawn(Il2CppObject* changeScene);
    static bool TeleportCar(Il2CppObject* car, Il2CppObject* destination, bool applyRotation);

    // Extra height added to the teleport destination so the car drops onto the
    // surface rather than clipping into it
    static constexpr float kTeleportHeight = 1.0f;

    // Destinations. Garages, car storage and meetspots are all changeScene
    // components, so they share one list and one "go to" action
    // (changeScene.goToMeetspot). drivingScene is scanned on the script thread;
    // garage entries are marked owned when GodConstant.owned_homeGarages
    // contains their homegarageSpotID
    enum class DestinationKind {
        Garage,
        CarStorage,
        Meetspot,
    };

    struct Destination {
        DestinationKind kind;
        int sourceIndex; // index within the source array, -1 when there is none
        int32_t id;      // homegarageSpotID or Meetspot id, 0 when unused
        std::string name;
        bool owned;
    };

    static void RequestDestinations();
    static void GoToDestination(Destination destination);
    static void BuyGarage(Destination destination);
    static void AddGarageToOwned(Destination destination);
    static void RemoveGarageFromOwned(Destination destination);
    static Il2CppObject* ResolveDestinationScene(Il2CppObject* god, const Destination& destination);
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

    // Fast travel targets, rebuilt on the script thread. The render thread only
    // reads the copy under the mutex
    static std::vector<TravelTarget> travelTargets;
    static std::mutex travelMutex;
    static std::atomic_bool travelRequested;
    static int travelIndex;

    // Destinations (garages, car storage, meetspots), rebuilt on the script
    // thread like the lists above
    static std::vector<Destination> destinationTargets;
    static std::mutex destinationMutex;
    static std::atomic_bool destinationRequested;
    static int destinationIndex;

    // Auto-refresh state; only ever touched from the script thread
    static bool wasSceneLoaded;

};
