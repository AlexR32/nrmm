#include "pch.h"

#include "world.h"
#include "player.h"
#include "enums.h"
#include "shared.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"

Shared::Status World::status("World");
World::Snapshot World::snapshot;

std::vector<World::MeetspotTarget> World::attackTargets;
std::mutex World::attackTargetsMutex;
std::atomic_bool World::attackTargetsRequested{false};
int World::attackTargetIndex = 0;
int32_t World::attackTargetId = -1;

std::vector<World::TravelTarget> World::travelTargets;
std::mutex World::travelMutex;
std::atomic_bool World::travelRequested{false};
int World::travelIndex = 0;

std::vector<World::Destination> World::destinationTargets;
std::mutex World::destinationMutex;
std::atomic_bool World::destinationRequested{false};
int World::destinationIndex = 0;

bool World::wasSceneLoaded = false;

// Meetspot attack

void World::RequestAttackTargets() {
    if (attackTargetsRequested.exchange(true)) return;

    MainThread::Post([]() {
        std::vector<MeetspotTarget> targets;

        Il2CppObject* god = Shared::God();
        if (god) {
            int32_t playerCrew = -1;
            Il2Cpp::GetInstanceFieldValue(god, "player_raceCrew", playerCrew);

            Il2CppObject* drivingScene = Il2Cpp::GetInstanceFieldObject(god, "floatingPoint_drivingScene");
            Il2CppObject* spotsObject = drivingScene ? Il2Cpp::GetInstanceFieldObject(drivingScene, "local_meetspots") : nullptr;
            Il2CppArray* spots = reinterpret_cast<Il2CppArray*>(spotsObject);

            if (spots) {
                const size_t count = Il2Cpp::ArrayLength(spots);
                for (size_t i = 0; i < count; ++i) {
                    Il2CppObject* entry = Il2Cpp::ArrayGetRef(spots, i);
                    if (!entry) continue;

                    int32_t spotId = -1;
                    if (!Il2Cpp::GetInstanceFieldValue(entry, "targetMeetSpot", spotId)) continue;

                    Il2CppObject* data = Player::LoadMeetspotData(spotId);
                    if (!data) continue;

                    int32_t owner = -1;
                    if (!Il2Cpp::GetInstanceFieldValue(data, "owner", owner)) continue;

                    // Only meetspots the player's crew owns are defendable
                    if (owner != playerCrew) continue;

                    targets.push_back({spotId, Player::MeetspotName(spotId)});
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(attackTargetsMutex);
            attackTargets = std::move(targets);
        }

        attackTargetsRequested.store(false, std::memory_order_release);
    });
}

// Old implementation
/*void Player::ForceMeetspotAttack() {
    MainThread::Post([]() {
        Il2CppClass* godClass = Il2Cpp::FindClass("GodConstant");
        if (!godClass) { status.Set("GodConstant not found"); return; }

        const MethodInfo* method = Il2Cpp::GetMethod(godClass, "rollForMeetspotAttack", 0);
        if (!method) { status.Set("rollForMeetspotAttack not found"); return; }

        Il2CppObject* god = Shared::God();

        Il2CppObject* enumerator = Il2Cpp::Invoke(method, god, nullptr);

        Il2Cpp::StartCoroutine(god, enumerator);

        //status.Set("Ended the night");
    });
}*/

void World::ForceMeetspotAttack() {
    const int32_t targetId = attackTargetId;

    MainThread::Post([targetId]() {
        if (targetId < 0) { status.Set("Select a meetspot to attack"); return; }

        Il2CppClass* godClass = Il2Cpp::FindClass("GodConstant");
        Il2CppObject* god = Shared::God();
        if (!godClass || !god) { status.Set("GodConstant not found"); return; }

        const MethodInfo* attack = Il2Cpp::GetMethod(godClass, "gameEvent_meetSpotAttack", 0);
        if (!attack) { status.Set("gameEvent_meetSpotAttack not found"); return; }

        int32_t playerCrew = -1;
        Il2Cpp::GetInstanceFieldValue(god, "player_raceCrew", playerCrew);

        Il2CppObject* drivingScene = Il2Cpp::GetInstanceFieldObject(god, "floatingPoint_drivingScene");
        Il2CppObject* spotsObject = drivingScene ? Il2Cpp::GetInstanceFieldObject(drivingScene, "local_meetspots") : nullptr;
        Il2CppArray* spots = reinterpret_cast<Il2CppArray*>(spotsObject);
        if (!spots) { status.Set("local_meetspots not available (not driving)"); return; }

        Il2CppObject* scene = nullptr;
        const size_t count = Il2Cpp::ArrayLength(spots);
        for (size_t i = 0; i < count; ++i) {
            Il2CppObject* entry = Il2Cpp::ArrayGetRef(spots, i);
            if (!entry) continue;

            int32_t spotId = -1;
            if (!Il2Cpp::GetInstanceFieldValue(entry, "targetMeetSpot", spotId)) continue;
            if (spotId != targetId) continue;

            Il2CppObject* data = Player::LoadMeetspotData(spotId);
            int32_t owner = -1;
            if (!data || !Il2Cpp::GetInstanceFieldValue(data, "owner", owner)) continue;
            if (owner != playerCrew) continue;

            scene = entry;
            break;
        }

        if (!scene) { status.Set("Meetspot not found in the current scene"); return; }

        if (!Il2Cpp::SetInstanceFieldObject(god, "meetSpot_underAttack", scene)) {
            status.Set("meetSpot_underAttack field not found");
            return;
        }

        Il2CppObject* result = Il2Cpp::Invoke(attack, god, nullptr);
        const bool triggered = Il2Cpp::UnboxBool(result);
        status.Set(triggered ? "Meetspot attack triggered at " + Player::MeetspotName(targetId) : "gameEvent_meetSpotAttack returned false");
    });
}

void World::ToggleTraffic(bool active) {
    MainThread::Post([active]() {
        Il2CppObject* tC = Shared::TC();
        if (!tC) { status.Set("TrafficCoordinator not found"); return; }

        Il2CppClass* tCClass = Il2Cpp::FindClass("PlanetJem.Roads.Traffic.TrafficCoordinator");
        const MethodInfo* method = Il2Cpp::GetMethod(tCClass, "SetTrafficEnabled", 2);
        if (!method) { status.Set("SetTrafficEnabled not found"); return; }

        bool activeArg = active;
        bool clearActive = !active;
        void* args[2] = {&activeArg, &clearActive};
        Il2Cpp::Invoke(method, tC, args);
    });
}

// Fast travel

void World::RequestTravelTargets() {
    if (travelRequested.exchange(true)) return;

    MainThread::Post([]() {
        std::vector<TravelTarget> targets;

        Il2CppObject* god = Shared::God();
        Il2CppObject* scene = god ? Il2Cpp::GetInstanceFieldObject(god, "floatingPoint_drivingScene") : nullptr;

        // Fast travel stations
        Il2CppClass* systemClass = Il2Cpp::FindClass("PlanetJem.Roads.FastTravel.FastTravelSystem");
        Il2CppObject* system = systemClass ? Il2Cpp::GetStaticFieldObject(systemClass, "instance_") : nullptr;
        Il2CppArray* trucks = system ? reinterpret_cast<Il2CppArray*>(Il2Cpp::GetInstanceFieldObject(system, "trucks_")) : nullptr;
        if (trucks) {
            const size_t count = Il2Cpp::ArrayLength(trucks);
            for (size_t i = 0; i < count; ++i) {
                Il2CppObject* truck = Il2Cpp::ArrayGetRef(trucks, i);
                if (!truck) continue;

                Il2CppObject* nameObject = Il2Cpp::GetInstanceFieldObject(truck, "displayName_");
                std::string name = nameObject ? Il2Cpp::StringToUtf8(reinterpret_cast<Il2CppString*>(nameObject)) : ("Station " + std::to_string(i));
                if (name.empty()) name = "Station " + std::to_string(i);
                targets.push_back({TravelKind::Truck, static_cast<int>(i), std::move(name)});
            }
        }

        // Garages
        Il2CppArray* garages = scene ? reinterpret_cast<Il2CppArray*>(Il2Cpp::GetInstanceFieldObject(scene, "homegarageSpot_garages")) : nullptr;
        if (garages) {
            const size_t count = Il2Cpp::ArrayLength(garages);
            for (size_t i = 0; i < count; ++i) {
                Il2CppObject* garage = Il2Cpp::ArrayGetRef(garages, i);
                if (!garage) continue;

                int32_t id = -1;
                if (!Il2Cpp::GetInstanceFieldValue(garage, "homegarageSpotID", id)) continue;

                targets.push_back({TravelKind::Garage, static_cast<int>(i), Enums::NameOf(Enums::Id::HouseSaveIds, id, "Garage ")});
            }
        }

        // Car storage
        if (Il2CppObject* storage = scene ? Il2Cpp::GetInstanceFieldObject(scene, "carStorage") : nullptr) {
            Il2CppObject* nameObject = Il2Cpp::GetInstanceFieldObject(storage, "changeSceneName");
            std::string name = nameObject ? Il2Cpp::StringToUtf8(reinterpret_cast<Il2CppString*>(nameObject)) : "";
            if (name.empty()) name = "Car Storage";
            targets.push_back({TravelKind::CarStorage, -1, std::move(name)});
        }

        // Meetspots
        Il2CppArray* meetspots = scene ? reinterpret_cast<Il2CppArray*>(Il2Cpp::GetInstanceFieldObject(scene, "local_meetspots")) : nullptr;
        if (meetspots) {
            const size_t count = Il2Cpp::ArrayLength(meetspots);
            for (size_t i = 0; i < count; ++i) {
                Il2CppObject* spot = Il2Cpp::ArrayGetRef(meetspots, i);
                if (!spot) continue;

                int32_t id = 0;
                Il2Cpp::GetInstanceFieldValue(spot, "targetMeetSpot", id);

                std::string name = id != 0 ? Player::MeetspotName(id) : std::string();
                if (name.empty()) {
                    Il2CppObject* nameObject = Il2Cpp::GetInstanceFieldObject(spot, "changeSceneName");
                    name = nameObject ? Il2Cpp::StringToUtf8(reinterpret_cast<Il2CppString*>(nameObject)) : "";
                }
                if (name.empty()) name = "Meetspot " + std::to_string(i);

                targets.push_back({TravelKind::Meetspot, static_cast<int>(i), std::move(name), nullptr, false});
            }
        }

        // Gas stations are changeScene components flagged with isGasStation
        if (Il2CppClass* changeSceneClass = Il2Cpp::FindClass("changeScene")) {
            Il2CppArray* gasStations = Il2Cpp::FindObjectsOfType(changeSceneClass);
            if (gasStations) {
                const size_t count = Il2Cpp::ArrayLength(gasStations);
                for (size_t i = 0; i < count; ++i) {
                    Il2CppObject* changeScene = Il2Cpp::ArrayGetRef(gasStations, i);
                    if (!changeScene) continue;

                    bool gasStation = false;
                    Il2Cpp::GetInstanceFieldValue(changeScene, "isGasStation", gasStation);
                    if (!gasStation) continue;

                    Il2CppObject* nameObject = Il2Cpp::GetInstanceFieldObject(changeScene, "gasStation_name");
                    std::string name = nameObject ? Il2Cpp::StringToUtf8(reinterpret_cast<Il2CppString*>(nameObject)) : "";
                    if (name.empty()) {
                        nameObject = Il2Cpp::GetInstanceFieldObject(changeScene, "changeSceneName");
                        name = nameObject ? Il2Cpp::StringToUtf8(reinterpret_cast<Il2CppString*>(nameObject)) : "";
                    }
                    if (name.empty()) name = "Gas Station " + std::to_string(i);

                    targets.push_back({TravelKind::GasStation, static_cast<int>(i), std::move(name), changeScene, false});
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(travelMutex);
            travelTargets = std::move(targets);
        }

        travelRequested.store(false, std::memory_order_release);
    });
}

// Resolves the world transform a travel target teleports the car to: the
// truck's destination marker, the changeScene jump-to-garage spawn point of a
// garage, car storage or gas station, or the enter_location child of a meetspot
Il2CppObject* World::ResolveTravelTransform(Il2CppObject* scene, const TravelTarget& target) {
    if (!scene) return nullptr;

    switch (target.kind) {
        case TravelKind::Truck: {
            Il2CppClass* systemClass = Il2Cpp::FindClass("PlanetJem.Roads.FastTravel.FastTravelSystem");
            Il2CppObject* system = systemClass ? Il2Cpp::GetStaticFieldObject(systemClass, "instance_") : nullptr;
            Il2CppArray* trucks = system ? reinterpret_cast<Il2CppArray*>(Il2Cpp::GetInstanceFieldObject(system, "trucks_")) : nullptr;
            if (!trucks || target.sourceIndex < 0 || static_cast<size_t>(target.sourceIndex) >= Il2Cpp::ArrayLength(trucks)) return nullptr;

            Il2CppObject* truck = Il2Cpp::ArrayGetRef(trucks, target.sourceIndex);
            return truck ? Il2Cpp::GetInstanceFieldObject(truck, "destination_") : nullptr;
        }
        case TravelKind::Garage: {
            Il2CppArray* garages = reinterpret_cast<Il2CppArray*>(Il2Cpp::GetInstanceFieldObject(scene, "homegarageSpot_garages"));
            if (!garages || target.sourceIndex < 0 || static_cast<size_t>(target.sourceIndex) >= Il2Cpp::ArrayLength(garages)) return nullptr;

            Il2CppObject* garage = Il2Cpp::ArrayGetRef(garages, target.sourceIndex);
            Il2CppObject* changeScene = garage ? Il2Cpp::GetInstanceFieldObject(garage, "garage_changeScene") : nullptr;
            return ResolveChangeSceneSpawn(changeScene);
        }
        case TravelKind::CarStorage: {
            return ResolveChangeSceneSpawn(Il2Cpp::GetInstanceFieldObject(scene, "carStorage"));
        }
        case TravelKind::Meetspot: {
            Il2CppArray* spots = reinterpret_cast<Il2CppArray*>(Il2Cpp::GetInstanceFieldObject(scene, "local_meetspots"));
            if (!spots || target.sourceIndex < 0 || static_cast<size_t>(target.sourceIndex) >= Il2Cpp::ArrayLength(spots)) return nullptr;

            Il2CppObject* changeScene = Il2Cpp::ArrayGetRef(spots, target.sourceIndex);
            if (!changeScene) return nullptr;

            // Meetspots carry their arrival point on a child named
            // enter_location_<n>; fall back to the changeScene spawn
            Il2CppObject* root = Il2Cpp::GetTransform(changeScene);
            if (Il2CppObject* enter = Shared::FindDescendantByNamePrefix(root, "enter_location_")) return enter;
            return ResolveChangeSceneSpawn(changeScene);
        }
        case TravelKind::GasStation: {
            Il2CppObject* changeScene = target.changeScene;
            if (!changeScene) return nullptr;

            // The gas station's enter_location_<n> marker sits under the
            // changeScene's parent, so search from there; fall back to the
            // changeScene spawn
            Il2CppObject* root = Il2Cpp::GetTransform(changeScene);
            Il2CppObject* parent = Shared::GetTransformParent(root);
            if (parent && Il2Cpp::IsUnityObjectAlive(parent)) root = parent;
            if (Il2CppObject* enter = Shared::FindDescendantByNamePrefix(root, "enter_location_")) return enter;
            return ResolveChangeSceneSpawn(changeScene);
        }
    }
    return nullptr;
}

// Prefers the changeScene's jumpToGarage_spawnPoint (where the car is placed
// when returning from a garage or car storage), falling back to the
// changeScene's own transform when the spawn point is unset
Il2CppObject* World::ResolveChangeSceneSpawn(Il2CppObject* changeScene) {
    if (!changeScene || !Il2Cpp::IsUnityObjectAlive(changeScene)) return nullptr;

    Il2CppObject* spawnPoint = Il2Cpp::GetInstanceFieldObject(changeScene, "jumpToGarage_spawnPoint");
    if (spawnPoint && Il2Cpp::IsUnityObjectAlive(spawnPoint)) return spawnPoint;

    return Il2Cpp::GetTransform(changeScene);
}

// Moves the player car onto the destination transform. Reproduces the car-side
// work drivingScene.fastTravel_Basic does (stop the rigidbody, freeze it, snap
// the transform to the destination pose, drop it back to dynamic) while
// skipping the travel cost and the fade/chunk-streaming cinematic
bool World::TeleportCar(Il2CppObject* car, Il2CppObject* destination, bool applyRotation) {
    if (!car || !Il2Cpp::IsUnityObjectAlive(destination)) return false;

    Il2CppObject* carTransform = Il2Cpp::GetTransform(car);
    if (!carTransform) return false;

    Il2CppClass* transformClass = Il2Cpp::FindClass("UnityEngine.Transform", Il2Cpp::GetImage("UnityEngine.CoreModule"));
    const MethodInfo* getPosition = transformClass ? Il2Cpp::GetMethod(transformClass, "get_position", 0) : nullptr;
    const MethodInfo* getRotation = transformClass ? Il2Cpp::GetMethod(transformClass, "get_rotation", 0) : nullptr;
    const MethodInfo* setPositionAndRotation = transformClass ? Il2Cpp::GetMethod(transformClass, "SetPositionAndRotation", 2) : nullptr;
    if (!getPosition || !getRotation || !setPositionAndRotation) return false;

    // get_position/get_rotation return value types, so Invoke hands back a
    // boxed Vector3/Quaternion whose payload doubles as the by-value argument.
    // The boxes are kept alive until the move completes. When the destination's
    // facing should not be adopted, keep the car's current rotation
    Il2CppObject* boxedPosition = Il2Cpp::Invoke(getPosition, destination, nullptr);
    Il2CppObject* boxedRotation = Il2Cpp::Invoke(getRotation, applyRotation ? destination : carTransform, nullptr);
    void* position = Il2Cpp::UnboxRaw(boxedPosition);
    void* rotation = Il2Cpp::UnboxRaw(boxedRotation);
    if (!position || !rotation) return false;

    // Lift the car slightly above the resolved point so it settles onto the
    // surface instead of clipping through it (Vector3.y is the second float)
    reinterpret_cast<float*>(position)[1] += kTeleportHeight;

    Il2CppObject* rigid = Il2Cpp::GetInstanceFieldObject(car, "rigid");
    Il2CppClass* rigidClass = rigid ? Il2Cpp::ObjectClass(rigid) : nullptr;

    float zero[3] = {0.0f, 0.0f, 0.0f};
    if (rigidClass) {
        if (const MethodInfo* setVelocity = Il2Cpp::GetMethod(rigidClass, "set_velocity", 1)) {
            void* args[1] = {zero};
            Il2Cpp::Invoke(setVelocity, rigid, args);
        }
        if (const MethodInfo* setAngular = Il2Cpp::GetMethod(rigidClass, "set_angularVelocity", 1)) {
            void* args[1] = {zero};
            Il2Cpp::Invoke(setAngular, rigid, args);
        }
        if (const MethodInfo* setKinematic = Il2Cpp::GetMethod(rigidClass, "set_isKinematic", 1)) {
            bool kinematic = true;
            void* args[1] = {&kinematic};
            Il2Cpp::Invoke(setKinematic, rigid, args);
        }
    }

    void* moveArgs[2] = {position, rotation};
    Il2Cpp::Invoke(setPositionAndRotation, carTransform, moveArgs);

    if (rigidClass) {
        if (const MethodInfo* setKinematic = Il2Cpp::GetMethod(rigidClass, "set_isKinematic", 1)) {
            bool kinematic = false;
            void* args[1] = {&kinematic};
            Il2Cpp::Invoke(setKinematic, rigid, args);
        }
    }

    // A teleport leaves the replay ring buffer pointing at the old position,
    // which draws a streak back across the map until it ages out
    if (Il2CppClass* gameMasterClass = Il2Cpp::FindClass("PlanetJem.Core.GameMaster")) {
        const MethodInfo* getReplay = Il2Cpp::GetMethod(gameMasterClass, "get_Replay", 0);
        Il2CppObject* replay = getReplay ? Il2Cpp::Invoke(getReplay, nullptr, nullptr) : nullptr;
        const MethodInfo* clear = replay ? Il2Cpp::GetMethod(Il2Cpp::ObjectClass(replay), "ClearBuffer", 0) : nullptr;
        if (clear) Il2Cpp::Invoke(clear, replay, nullptr);
    }

    int32_t gear = 0;
    Il2Cpp::SetInstanceFieldValue(car, "currentGear", gear);
    return true;
}

void World::Travel() {
    const int index = travelIndex;

    MainThread::Post([index]() {
        Il2CppObject* god = Shared::God();
        if (!god) { status.Set("GodConstant not found"); return; }

        Il2CppObject* scene = Il2Cpp::GetInstanceFieldObject(god, "floatingPoint_drivingScene");
        if (!scene) { status.Set("Not in the driving scene"); return; }

        TravelTarget target;
        {
            std::lock_guard<std::mutex> lock(travelMutex);
            if (index < 0 || static_cast<size_t>(index) >= travelTargets.size()) {
                status.Set("Travel point not found (refresh the list)");
                return;
            }
            target = travelTargets[index];
        }

        Il2CppObject* destination = ResolveTravelTransform(scene, target);
        if (!destination) { status.Set("Travel destination missing"); return; }

        Il2CppObject* car = Il2Cpp::GetInstanceFieldObject(scene, "playerCar");
        if (!car) car = Shared::PlayerCar();
        if (!car) { status.Set("Player car not found"); return; }

        status.Set(TeleportCar(car, destination, target.applyRotation) ? "Fast traveled to " + target.name : "Travel failed");
    });
}

// Destinations

void World::RequestDestinations() {
    if (destinationRequested.exchange(true)) return;

    MainThread::Post([]() {
        std::vector<Destination> targets;

        Il2CppObject* god = Shared::God();
        Il2CppObject* scene = god ? Il2Cpp::GetInstanceFieldObject(god, "floatingPoint_drivingScene") : nullptr;

        // Garages, marked owned when GodConstant.owned_homeGarages contains
        // their homegarageSpotID
        Il2CppArray* garages = scene ? reinterpret_cast<Il2CppArray*>(Il2Cpp::GetInstanceFieldObject(scene, "homegarageSpot_garages")) : nullptr;
        if (garages) {
            Il2CppObject* ownedGarages = Il2Cpp::GetInstanceFieldObject(god, "owned_homeGarages");
            Il2CppClass* listClass = ownedGarages ? Il2Cpp::ObjectClass(ownedGarages) : nullptr;
            const MethodInfo* contains = listClass ? Il2Cpp::GetMethod(listClass, "Contains", 1) : nullptr;

            const size_t count = Il2Cpp::ArrayLength(garages);
            for (size_t i = 0; i < count; ++i) {
                Il2CppObject* garage = Il2Cpp::ArrayGetRef(garages, i);
                if (!garage) continue;

                int32_t id = -1;
                if (!Il2Cpp::GetInstanceFieldValue(garage, "homegarageSpotID", id)) continue;

                bool owned = false;
                if (contains) {
                    void* args[1] = {&id};
                    owned = Il2Cpp::UnboxBool(Il2Cpp::Invoke(contains, ownedGarages, args));
                }

                targets.push_back({DestinationKind::Garage, static_cast<int>(i), id, Enums::NameOf(Enums::Id::HouseSaveIds, id, "Garage "), owned});
            }
        }

        // Car storage is a single changeScene
        if (Il2CppObject* storage = scene ? Il2Cpp::GetInstanceFieldObject(scene, "carStorage") : nullptr) {
            Il2CppObject* nameObject = Il2Cpp::GetInstanceFieldObject(storage, "changeSceneName");
            std::string name = nameObject ? Il2Cpp::StringToUtf8(reinterpret_cast<Il2CppString*>(nameObject)) : "";
            if (name.empty()) name = "Car Storage";
            targets.push_back({DestinationKind::CarStorage, -1, 0, std::move(name), false});
        }

        // Meetspots are changeScene entries too
        Il2CppArray* meetspots = scene ? reinterpret_cast<Il2CppArray*>(Il2Cpp::GetInstanceFieldObject(scene, "local_meetspots")) : nullptr;
        if (meetspots) {
            const size_t count = Il2Cpp::ArrayLength(meetspots);
            for (size_t i = 0; i < count; ++i) {
                Il2CppObject* spot = Il2Cpp::ArrayGetRef(meetspots, i);
                if (!spot) continue;

                int32_t id = 0;
                Il2Cpp::GetInstanceFieldValue(spot, "targetMeetSpot", id);

                std::string name = id != 0 ? Player::MeetspotName(id) : std::string();
                if (name.empty()) {
                    Il2CppObject* nameObject = Il2Cpp::GetInstanceFieldObject(spot, "changeSceneName");
                    name = nameObject ? Il2Cpp::StringToUtf8(reinterpret_cast<Il2CppString*>(nameObject)) : "";
                }
                if (name.empty()) name = "Meetspot " + std::to_string(i);

                targets.push_back({DestinationKind::Meetspot, static_cast<int>(i), id, std::move(name), false});
            }
        }

        {
            std::lock_guard<std::mutex> lock(destinationMutex);
            destinationTargets = std::move(targets);
        }

        destinationRequested.store(false, std::memory_order_release);
    });
}

Il2CppObject* World::ResolveDestinationScene(Il2CppObject* god, const Destination& destination) {
    Il2CppObject* scene = god ? Il2Cpp::GetInstanceFieldObject(god, "floatingPoint_drivingScene") : nullptr;
    if (!scene) return nullptr;

    switch (destination.kind) {
        case DestinationKind::Garage: {
            Il2CppArray* garages = reinterpret_cast<Il2CppArray*>(Il2Cpp::GetInstanceFieldObject(scene, "homegarageSpot_garages"));
            if (!garages || destination.sourceIndex < 0 || static_cast<size_t>(destination.sourceIndex) >= Il2Cpp::ArrayLength(garages)) return nullptr;

            Il2CppObject* garage = Il2Cpp::ArrayGetRef(garages, destination.sourceIndex);
            return garage ? Il2Cpp::GetInstanceFieldObject(garage, "garage_changeScene") : nullptr;
        }
        case DestinationKind::CarStorage:
            return Il2Cpp::GetInstanceFieldObject(scene, "carStorage");
        case DestinationKind::Meetspot: {
            Il2CppArray* spots = reinterpret_cast<Il2CppArray*>(Il2Cpp::GetInstanceFieldObject(scene, "local_meetspots"));
            if (!spots || destination.sourceIndex < 0 || static_cast<size_t>(destination.sourceIndex) >= Il2Cpp::ArrayLength(spots)) return nullptr;

            return Il2Cpp::ArrayGetRef(spots, destination.sourceIndex);
        }
    }
    return nullptr;
}

void World::GoToDestination(Destination destination) {
    MainThread::Post([destination]() {
        Il2CppObject* god = Shared::God();
        if (!god) { status.Set("GodConstant not found"); return; }

        Il2CppObject* scene = ResolveDestinationScene(god, destination);
        if (!scene) { status.Set(destination.name + " not found (refresh the list)"); return; }

        const MethodInfo* method = Il2Cpp::GetMethod(Il2Cpp::FindClass("changeScene"), "goToMeetspot", 0);
        if (!method) { status.Set("goToMeetspot not found"); return; }

        Il2Cpp::Invoke(method, scene, nullptr);
        status.Set("Traveling to " + destination.name);
    });
}

void World::BuyGarage(Destination destination) {
    MainThread::Post([destination]() {
        Il2CppObject* god = Shared::God();
        if (!god) { status.Set("GodConstant not found"); return; }

        Il2CppObject* garageScene = ResolveDestinationScene(god, destination);
        if (!garageScene) { status.Set("Garage not found (refresh the list)"); return; }

        Il2CppObject* uiData = Il2Cpp::GetInstanceFieldObject(god, "UI_Data");
        const MethodInfo* method = Il2Cpp::GetMethod(Il2Cpp::FindClass("RCC_DashboardInputs"), "buyHouseStart", 1);
        if (!uiData || !method) { status.Set("buyHouseStart not found"); return; }

        void* args[1] = {garageScene};
        Il2Cpp::Invoke(method, uiData, args);
        status.Set("Started the garage purchase");
    });
}

bool World::SaveOwnedGarages(Il2CppObject* god, Il2CppObject* ownedGarages) {
    Il2CppClass* es3Class = Il2Cpp::FindClass("ES3", Il2Cpp::GetImage("Assembly-CSharp-firstpass"));
    if (!es3Class || !ownedGarages) return false;

    const MethodInfo* save = Il2Cpp::GetMethodByParamClass(es3Class, "Save", 3, 2, "ES3Settings");
    if (!save) return false;

    void* key = Il2Cpp::NewString("ownedHomeGarages_saveIDList");
    Il2CppObject* settings = Il2Cpp::GetInstanceFieldObject(god, "es3_settings");
    if (!key || !settings) return false;

    void* args[3] = {key, ownedGarages, settings};
    Il2Cpp::Invoke(save, nullptr, args);
    return true;
}

void World::AddGarageToOwned(Destination destination) {
    MainThread::Post([destination]() {
        Il2CppObject* god = Shared::God();
        if (!god) { status.Set("GodConstant not found"); return; }

        Il2CppObject* ownedGarages = Il2Cpp::GetInstanceFieldObject(god, "owned_homeGarages");
        if (!ownedGarages) { status.Set("owned_homeGarages not loaded"); return; }

        int32_t id = destination.id;

        Il2CppClass* listClass = Il2Cpp::ObjectClass(ownedGarages);
        const MethodInfo* contains = listClass ? Il2Cpp::GetMethod(listClass, "Contains", 1) : nullptr;
        const MethodInfo* add = listClass ? Il2Cpp::GetMethod(listClass, "Add", 1) : nullptr;
        if (!contains || !add) { status.Set("owned_homeGarages methods not found"); return; }

        void* args[1] = {&id};
        if (Il2Cpp::UnboxBool(Il2Cpp::Invoke(contains, ownedGarages, args))) {
            status.Set("Garage already owned");
            return;
        }

        Il2Cpp::Invoke(add, ownedGarages, args);
        status.Set(SaveOwnedGarages(god, ownedGarages) ? "Added garage to owned" : "Added garage but the save failed");

        RequestDestinations();
    });
}

void World::RemoveGarageFromOwned(Destination destination) {
    MainThread::Post([destination]() {
        Il2CppObject* god = Shared::God();
        if (!god) { status.Set("GodConstant not found"); return; }

        Il2CppObject* ownedGarages = Il2Cpp::GetInstanceFieldObject(god, "owned_homeGarages");
        if (!ownedGarages) { status.Set("owned_homeGarages not loaded"); return; }

        int32_t id = destination.id;

        Il2CppClass* listClass = Il2Cpp::ObjectClass(ownedGarages);
        const MethodInfo* remove = listClass ? Il2Cpp::GetMethod(listClass, "Remove", 1) : nullptr;
        if (!remove) { status.Set("owned_homeGarages Remove not found"); return; }

        void* args[1] = {&id};
        if (!Il2Cpp::UnboxBool(Il2Cpp::Invoke(remove, ownedGarages, args))) {
            status.Set("Garage was not owned");
            return;
        }

        status.Set(SaveOwnedGarages(god, ownedGarages) ? "Removed garage from owned" : "Removed garage but the save failed");

        RequestDestinations();
    });
}

// Snapshot / tab

void World::RefreshSnapshot() {
    Il2Cpp::ThreadAttach();

    // Traffic
    Il2CppObject* tC = Shared::TC();
    snapshot.trafficAvailable.store(tC != nullptr, std::memory_order_relaxed);
    if (tC) {
        bool trafficEnabled = true;
        if (Il2Cpp::GetInstanceFieldValue(tC, "trafficEnabled_", trafficEnabled)) {
            snapshot.trafficEnabled.store(trafficEnabled, std::memory_order_relaxed);
        }
    }

    // Auto-Refresh
    Il2CppObject* god = Shared::God();
    Il2CppObject* scene = god ? Il2Cpp::GetInstanceFieldObject(god, "floatingPoint_drivingScene") : nullptr;
    const bool sceneLoaded = scene != nullptr;
    snapshot.sceneAvailable.store(sceneLoaded, std::memory_order_relaxed);

    if (sceneLoaded) {
        if (!wasSceneLoaded) {
            wasSceneLoaded = true;
            RequestAttackTargets();
            RequestTravelTargets();
            RequestDestinations();
        }
    } else if (wasSceneLoaded) {
        // Left the driving scene: the lists no longer describe anything valid
        wasSceneLoaded = false;
        {
            std::lock_guard<std::mutex> lock(attackTargetsMutex);
            attackTargets.clear();
        }
        {
            std::lock_guard<std::mutex> lock(travelMutex);
            travelTargets.clear();
        }
        {
            std::lock_guard<std::mutex> lock(destinationMutex);
            destinationTargets.clear();
        }
    }
}

void World::RenderTab() {
    if (!Shared::BeginGameTab("World")) return;

    const bool sceneAvailable = snapshot.sceneAvailable.load(std::memory_order_relaxed);
    const bool trafficAvailable = snapshot.trafficAvailable.load(std::memory_order_relaxed);

    if (!trafficAvailable || !sceneAvailable) {
        Shared::RenderLabel("NOT IN OPEN WORLD");
        return;
    };

    status.Render();

    // Traffic
    bool trafficEnabled = snapshot.trafficEnabled.load(std::memory_order_relaxed);
    ImGui::BeginDisabled(!trafficAvailable);
    if (ImGui::Checkbox("Toggle Traffic", &trafficEnabled)) {
        ToggleTraffic(trafficEnabled);
    }
    ImGui::EndDisabled();

    ImGui::SeparatorText("Meetspot attack");

    // The target list is rebuilt on the script thread; take a copy so the
    // widgets never touch the vector the script thread may replace.
    std::vector<MeetspotTarget> targets;
    {
        std::lock_guard<std::mutex> lock(attackTargetsMutex);
        targets = attackTargets;
    }

    if (targets.empty()) {
        ImGui::TextUnformatted(sceneAvailable ? "No defendable meetspots" : "Not in a driving scene");
        if (sceneAvailable) {
            ImGui::SameLine();
            if (ImGui::Button("Refresh##attackTargets")) {
                RequestAttackTargets();
            }
        }
    } else {
        if (attackTargetIndex < 0 || attackTargetIndex >= static_cast<int>(targets.size())) attackTargetIndex = 0;
        attackTargetId = targets[attackTargetIndex].id;

        const char* preview = targets[attackTargetIndex].name.c_str();
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::BeginCombo("##meetspotAttack", preview)) {
            for (int i = 0; i < static_cast<int>(targets.size()); ++i) {
                const bool selected = (i == attackTargetIndex);
                if (ImGui::Selectable(targets[i].name.c_str(), selected)) {
                    attackTargetIndex = i;
                    attackTargetId = targets[i].id;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Trigger Attack")) {
            ForceMeetspotAttack();
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh##attackTargets")) {
            RequestAttackTargets();
        }
    }

    ImGui::SeparatorText("Fast travel");

    // The target list is rebuilt on the script thread; copy it so the widgets
    // never touch the vector while it is being replaced.
    std::vector<TravelTarget> travels;
    {
        std::lock_guard<std::mutex> lock(travelMutex);
        travels = travelTargets;
    }

    if (travels.empty()) {
        ImGui::TextUnformatted(sceneAvailable ? "No travel points" : "Not in a driving scene");
        if (sceneAvailable) {
            ImGui::SameLine();
            if (ImGui::Button("Refresh##travelPoints")) {
                RequestTravelTargets();
            }
        }
    } else {
        if (travelIndex < 0 || travelIndex >= static_cast<int>(travels.size())) travelIndex = 0;

        const char* preview = travels[travelIndex].name.c_str();
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::BeginCombo("##travelPoint", preview)) {
            for (int i = 0; i < static_cast<int>(travels.size()); ++i) {
                const bool selected = (i == travelIndex);
                // Names can repeat (garages, meetspots), so scope each entry's
                // ID by its index to keep the selectables distinct.
                ImGui::PushID(i);
                if (ImGui::Selectable(travels[i].name.c_str(), selected)) {
                    travelIndex = i;
                }
                if (selected) ImGui::SetItemDefaultFocus();
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::Button("Travel")) {
            Travel();
        }
    }

    ImGui::SeparatorText("Destinations");

    // The destination list is rebuilt on the script thread; copy it so the
    // widgets never touch the vector while it is being replaced.
    std::vector<Destination> destinations;
    {
        std::lock_guard<std::mutex> lock(destinationMutex);
        destinations = destinationTargets;
    }

    if (destinations.empty()) {
        ImGui::TextUnformatted(sceneAvailable ? "No destinations" : "Not in a driving scene");
        if (sceneAvailable) {
            ImGui::SameLine();
            if (ImGui::Button("Refresh##destinations")) {
                RequestDestinations();
            }
        }
    } else {
        if (destinationIndex < 0 || destinationIndex >= static_cast<int>(destinations.size())) destinationIndex = 0;

        // Only garages can be owned; the tag in the dropdown tells the button
        // row whether to offer Go or Buy / Add
        const Destination& current = destinations[destinationIndex];
        const bool garage = current.kind == DestinationKind::Garage;
        std::string preview = current.name + (garage && current.owned ? " (owned)" : "");
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::BeginCombo("##destinationCombo", preview.c_str())) {
            for (int i = 0; i < static_cast<int>(destinations.size()); ++i) {
                const bool selected = (i == destinationIndex);
                const Destination& entry = destinations[i];
                const std::string label = entry.name + (entry.kind == DestinationKind::Garage && entry.owned ? " (owned)" : "");
                if (ImGui::Selectable(label.c_str(), selected)) {
                    destinationIndex = i;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (garage && current.owned) {
            if (ImGui::Button("Go")) {
                GoToDestination(current);
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove")) {
                RemoveGarageFromOwned(current);
            }
        } else if (garage) {
            if (ImGui::Button("Buy")) {
                BuyGarage(current);
            }
            ImGui::SameLine();
            if (ImGui::Button("Add")) {
                AddGarageToOwned(current);
            }
        } else if (ImGui::Button("Go")) {
            GoToDestination(current);
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh##destinations")) {
            RequestDestinations();
        }
    }
}
