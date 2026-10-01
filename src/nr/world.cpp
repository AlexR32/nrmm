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

std::vector<std::string> World::travelTargets;
std::mutex World::travelMutex;
std::atomic_bool World::travelRequested{false};
int World::travelIndex = 0;

std::vector<World::GarageTarget> World::garageTargets;
std::mutex World::garageMutex;
std::atomic_bool World::garageRequested{false};
int World::garageIndex = 0;

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
        std::vector<std::string> names;

        Il2CppClass* systemClass = Il2Cpp::FindClass("PlanetJem.Roads.FastTravel.FastTravelSystem");
        Il2CppObject* system = systemClass ? Il2Cpp::GetStaticFieldObject(systemClass, "instance_") : nullptr;
        if (system) {
            Il2CppObject* trucksObject = Il2Cpp::GetInstanceFieldObject(system, "trucks_");
            Il2CppArray* trucks = reinterpret_cast<Il2CppArray*>(trucksObject);

            if (trucks) {
                const size_t count = Il2Cpp::ArrayLength(trucks);
                for (size_t i = 0; i < count; ++i) {
                    Il2CppObject* truck = Il2Cpp::ArrayGetRef(trucks, i);
                    if (!truck) continue;

                    Il2CppObject* nameObject = Il2Cpp::GetInstanceFieldObject(truck, "displayName_");
                    std::string name = nameObject ? Il2Cpp::StringToUtf8(reinterpret_cast<Il2CppString*>(nameObject)) : ("Truck " + std::to_string(i));
                    if (name.empty()) name = "Truck " + std::to_string(i);
                    names.push_back(std::move(name));
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(travelMutex);
            travelTargets = std::move(names);
        }

        travelRequested.store(false, std::memory_order_release);
    });
}

void World::Travel() {
    const int index = travelIndex;

    MainThread::Post([index]() {
        Il2CppObject* god = Shared::God();
        if (!god) { status.Set("GodConstant not found"); return; }

        Il2CppObject* scene = Il2Cpp::GetInstanceFieldObject(god, "floatingPoint_drivingScene");
        if (!scene) { status.Set("Not in the driving scene"); return; }

        Il2CppClass* systemClass = Il2Cpp::FindClass("PlanetJem.Roads.FastTravel.FastTravelSystem");
        Il2CppObject* system = systemClass ? Il2Cpp::GetStaticFieldObject(systemClass, "instance_") : nullptr;
        if (!system) { status.Set("FastTravelSystem not found"); return; }

        Il2CppObject* trucksObject = Il2Cpp::GetInstanceFieldObject(system, "trucks_");
        Il2CppArray* trucks = reinterpret_cast<Il2CppArray*>(trucksObject);
        if (!trucks || index < 0 || static_cast<size_t>(index) >= Il2Cpp::ArrayLength(trucks)) {
            status.Set("Travel point not found (refresh the list)");
            return;
        }

        Il2CppObject* truck = Il2Cpp::ArrayGetRef(trucks, index);
        Il2CppObject* destination = truck ? Il2Cpp::GetInstanceFieldObject(truck, "destination_") : nullptr;
        if (!destination) { status.Set("Travel destination missing"); return; }

        Il2CppClass* sceneClass = Il2Cpp::FindClass("drivingScene");
        const MethodInfo* method = Il2Cpp::GetMethod(sceneClass, "fastTravel_Basic", 2);
        if (!method) { status.Set("fastTravel_Basic not found"); return; }

        // The mod menu travels for free and without the tow truck light show
        int32_t cost = 0;
        void* args[2] = {&cost, destination};
        Il2CppObject* enumerator = Il2Cpp::Invoke(method, scene, args);
        if (!enumerator) { status.Set("fastTravel_Basic returned null"); return; }

        Il2Cpp::StartCoroutine(scene, enumerator);
        status.Set("Fast traveling...");
    });
}

// Garage

void World::RequestGarageTargets() {
    if (garageRequested.exchange(true)) return;

    MainThread::Post([]() {
        std::vector<GarageTarget> targets;

        Il2CppObject* god = Shared::God();
        Il2CppObject* scene = god ? Il2Cpp::GetInstanceFieldObject(god, "floatingPoint_drivingScene") : nullptr;
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

                targets.push_back({id, Enums::NameOf(Enums::Id::HouseSaveIds, id, "Garage "), owned});
            }
        }

        {
            std::lock_guard<std::mutex> lock(garageMutex);
            garageTargets = std::move(targets);
        }

        garageRequested.store(false, std::memory_order_release);
    });
}

Il2CppObject* World::ResolveGarageScene(Il2CppObject* god, int index) {
    Il2CppObject* scene = god ? Il2Cpp::GetInstanceFieldObject(god, "floatingPoint_drivingScene") : nullptr;
    Il2CppArray* garages = scene ? reinterpret_cast<Il2CppArray*>(Il2Cpp::GetInstanceFieldObject(scene, "homegarageSpot_garages")) : nullptr;
    if (!garages || index < 0 || static_cast<size_t>(index) >= Il2Cpp::ArrayLength(garages)) return nullptr;

    Il2CppObject* garage = Il2Cpp::ArrayGetRef(garages, index);
    return garage ? Il2Cpp::GetInstanceFieldObject(garage, "garage_changeScene") : nullptr;
}

void World::GoToGarage(int index) {
    MainThread::Post([index]() {
        Il2CppObject* god = Shared::God();
        if (!god) { status.Set("GodConstant not found"); return; }

        Il2CppObject* garageScene = ResolveGarageScene(god, index);
        if (!garageScene) { status.Set("Garage not found (refresh the list)"); return; }

        const MethodInfo* method = Il2Cpp::GetMethod(Il2Cpp::FindClass("changeScene"), "goToMeetspot", 0);
        if (!method) { status.Set("goToMeetspot not found"); return; }

        Il2Cpp::Invoke(method, garageScene, nullptr);
        status.Set("Traveling to garage");
    });
}

void World::BuyGarage(int index) {
    MainThread::Post([index]() {
        Il2CppObject* god = Shared::God();
        if (!god) { status.Set("GodConstant not found"); return; }

        Il2CppObject* garageScene = ResolveGarageScene(god, index);
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

void World::AddGarageToOwned(int index) {
    MainThread::Post([index]() {
        Il2CppObject* god = Shared::God();
        if (!god) { status.Set("GodConstant not found"); return; }

        Il2CppObject* ownedGarages = Il2Cpp::GetInstanceFieldObject(god, "owned_homeGarages");
        if (!ownedGarages) { status.Set("owned_homeGarages not loaded"); return; }

        Il2CppObject* garageScene = ResolveGarageScene(god, index);
        if (!garageScene) { status.Set("Garage not found (refresh the list)"); return; }
        int32_t id = -1;
        if (!Il2Cpp::GetInstanceFieldValue(garageScene, "homegarageSpotID", id)) {
            status.Set("homegarageSpotID not found");
            return;
        }

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

        RequestGarageTargets();
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
            RequestGarageTargets();
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
            std::lock_guard<std::mutex> lock(garageMutex);
            garageTargets.clear();
        }
    }
}

void World::RenderTab() {
    if (!Shared::BeginGameTab("World")) return;

    const bool sceneAvailable = snapshot.sceneAvailable.load(std::memory_order_relaxed);
    const bool trafficAvailable = snapshot.trafficAvailable.load(std::memory_order_relaxed);

    if (!trafficAvailable || !sceneAvailable) {
        Shared::RenderLabel("NOT IN OPEN WORLD");
        ImGui::EndTabItem();
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

    // The truck list is rebuilt on the script thread; copy it so the widgets
    // never touch the vector while it is being replaced.
    std::vector<std::string> travels;
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

        const char* preview = travels[travelIndex].c_str();
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::BeginCombo("##travelPoint", preview)) {
            for (int i = 0; i < static_cast<int>(travels.size()); ++i) {
                const bool selected = (i == travelIndex);
                // Truck display names can repeat, so scope each entry's ID by
                // its index to keep the selectables distinct.
                ImGui::PushID(i);
                if (ImGui::Selectable(travels[i].c_str(), selected)) {
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

    ImGui::SeparatorText("Garages");

    // The garage list is rebuilt on the script thread; copy it so the widgets
    // never touch the vector while it is being replaced.
    std::vector<GarageTarget> garages;
    {
        std::lock_guard<std::mutex> lock(garageMutex);
        garages = garageTargets;
    }

    if (garages.empty()) {
        ImGui::TextUnformatted(sceneAvailable ? "No garages" : "Not in a driving scene");
        if (sceneAvailable) {
            ImGui::SameLine();
            if (ImGui::Button("Refresh##garages")) {
                RequestGarageTargets();
            }
        }
    } else {
        if (garageIndex < 0 || garageIndex >= static_cast<int>(garages.size())) garageIndex = 0;

        // Owned entries are marked in the dropdown so the button row below
        // knows whether to offer Go or Buy / Add
        std::string preview = garages[garageIndex].name + (garages[garageIndex].owned ? " (owned)" : "");
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::BeginCombo("##garageCombo", preview.c_str())) {
            for (int i = 0; i < static_cast<int>(garages.size()); ++i) {
                const bool selected = (i == garageIndex);
                const std::string label = garages[i].name + (garages[i].owned ? " (owned)" : "");
                if (ImGui::Selectable(label.c_str(), selected)) {
                    garageIndex = i;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (garages[garageIndex].owned) {
            if (ImGui::Button("Go")) {
                GoToGarage(garageIndex);
            }
        } else {
            if (ImGui::Button("Buy")) {
                BuyGarage(garageIndex);
            }
            ImGui::SameLine();
            if (ImGui::Button("Add")) {
                AddGarageToOwned(garageIndex);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh##garages")) {
            RequestGarageTargets();
        }
    }

    ImGui::EndTabItem();
}
