#include "pch.h"

#include "auction.h"
#include "shared.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"
#include "core/logger.h"

Shared::Status Auction::status("Auction");
Auction::Snapshot Auction::snapshot;
bool Auction::refreshPending = false;

// Destroy the instantiated papers and cars, clear the bookkeeping lists and let
// the game rebuild the listing
void Auction::RefreshAuction() {
    MainThread::Post([]() {
        Il2CppObject* carAuction = Shared::CarAuction();
        if (!carAuction) {
            status.Set("Enter the auction first");
            return;
        }

        // The prefab itself must never be destroyed
        Il2CppObject* prefab = Il2Cpp::GetInstanceFieldObject(carAuction, "carAuctionPaper_prefab");
        Il2CppObject* prefabGameObject = prefab ? Il2Cpp::GetGameObject(prefab) : nullptr;

        // Delete instantiated auction papers only
        Il2CppObject* papers = Il2Cpp::GetInstanceFieldObject(carAuction, "all_carAuctionPapers");
        const int paperCount = Shared::ListCount(papers);
        for (int i = 0; i < paperCount; ++i) {
            Il2CppObject* paper = Shared::ListGet(papers, i);
            if (!paper) continue;

            Il2CppObject* paperGameObject = Il2Cpp::GetGameObject(paper);
            if (paperGameObject && paperGameObject != prefabGameObject) {
                Shared::DestroyObject(paperGameObject);
            }
        }

        Il2CppObject* cars = Il2Cpp::GetInstanceFieldObject(carAuction, "carAuction_cars");
        const int carCount = Shared::ListCount(cars);
        for (int i = 0; i < carCount; ++i) {
            Il2CppObject* car = Shared::ListGet(cars, i);
            if (!car) continue;

            Il2CppObject* carGameObject = Il2Cpp::GetGameObject(car);
            if (carGameObject) Shared::DestroyObject(carGameObject);
        }

        // Clear old auction data
        Shared::ListClear(papers);
        Shared::ListClear(cars);
        Shared::ListClear(Il2Cpp::GetInstanceFieldObject(carAuction, "carsSaveList"));

        // Bypass loading saved cars and load new ones
        int32_t lastRefresh = -1;
        Il2Cpp::SetInstanceFieldValue(carAuction, "lastRefresh", lastRefresh);

        Il2CppClass* auctionClass = Il2Cpp::FindClass("customization_carAuction");
        const MethodInfo* setup = Il2Cpp::GetMethod(auctionClass, "setupCarsForSale", 0);
        if (setup) {
            Il2Cpp::Invoke(setup, carAuction, nullptr);
            // setupCarsForSale starts the spawning coroutine, which sets
            // waitForCarSpawning immediately; the status then flips to
            // "Auction refreshed" from RefreshSnapshot once it finishes
            status.Set("Refreshing...");
            refreshPending = true;
        } else {
            status.Set("setupCarsForSale not found");
        }
    });
}

void Auction::UnlockAllAuctionCars() {
    MainThread::Post([]() {
        Il2CppObject* carAuction = Shared::CarAuction();
        if (!carAuction) {
            status.Set("Enter the auction first");
            return;
        }

        Il2CppClass* chassisEnum = Il2Cpp::FindClass("car_carOrigin.ChassisType");
        if (!chassisEnum) { status.Set("ChassisType enum not found"); return; }

        // Only unlock chassis that have a car origin. The game passes this list
        // to spawnShopCar, whose coroutine never completes for a chassis with no
        // origin, so unlocking the whole enum makes the auction stall
        std::unordered_set<int32_t> validChassis;
        std::unordered_map<int32_t, std::vector<int32_t>> chassisByModel;
        if (!Shared::CollectCarOrigins(validChassis, chassisByModel)) {
            status.Set("Car origins not loaded yet");
            return;
        }

        std::vector<Il2Cpp::EnumMember> members = Il2Cpp::GetEnumMembers(chassisEnum);

        std::vector<const Il2Cpp::EnumMember*> values;
        values.reserve(validChassis.size());
        for (const Il2Cpp::EnumMember& member : members) {
            if (member.name == "null_type" || member.name == "generic") continue;
            if (validChassis.find(Shared::EnumValue(member)) == validChassis.end()) continue;
            values.push_back(&member);
        }
        if (values.empty()) { status.Set("No spawnable chassis found"); return; }

        Il2CppArray* unlocked = Il2Cpp::NewArray(chassisEnum, values.size());
        if (!unlocked) { status.Set("Failed to allocate the unlocked array"); return; }

        for (size_t i = 0; i < values.size(); ++i) {
            Il2Cpp::ArraySetRaw(unlocked, i, values[i]->raw.data(), values[i]->raw.size());
        }

        if (!Il2Cpp::SetInstanceFieldObject(carAuction, "UNLOCKED_CARS", reinterpret_cast<Il2CppObject*>(unlocked))) {
            status.Set("UNLOCKED_CARS field not found");
            return;
        }

        Il2CppObject* stored = Il2Cpp::GetInstanceFieldObject(carAuction, "UNLOCKED_CARS");
        if (stored != reinterpret_cast<Il2CppObject*>(unlocked)) {
            status.Set("Failed to store the unlocked array");
            return;
        }

        status.Set("Unlocked " + std::to_string(values.size()) + " cars");
    });
}

// Snapshot / tab

void Auction::RefreshSnapshot() {
    Il2Cpp::ThreadAttach();

    Il2CppObject* carAuction = Shared::CarAuction();
    snapshot.available.store(carAuction != nullptr, std::memory_order_relaxed);
    if (!carAuction) {
        snapshot.active.store(false, std::memory_order_relaxed);
        snapshot.mode.store(-1, std::memory_order_relaxed);
        snapshot.refreshing.store(false, std::memory_order_relaxed);
        return;
    }

    // The auction GameObject can stay referenced after it has been switched
    // off, so the mode alone is not enough to tell whether it is really open
    snapshot.active.store(Il2Cpp::IsActiveSelf(carAuction), std::memory_order_relaxed);

    int32_t mode = -1;
    Il2Cpp::GetInstanceFieldValue(carAuction, "carAuctionMode", mode);
    snapshot.mode.store(mode, std::memory_order_relaxed);

    // The game sets waitForCarSpawning for the whole span of getNewCarsForSale
    // and loadCarsForSale and clears it once the listing has been rebuilt
    bool waitingForCars = false;
    Il2Cpp::GetInstanceFieldValue(carAuction, "waitForCarSpawning", waitingForCars);
    snapshot.refreshing.store(waitingForCars, std::memory_order_relaxed);

    // Close out a mod-triggered refresh once the game has finished spawning
    if (refreshPending && !waitingForCars) {
        refreshPending = false;
        status.Set("Auction refreshed");
    }
}

void Auction::RenderTab() {
    if (!Shared::BeginGameTab("Auction")) return;

    status.Render();

    // The auction only accepts mod actions while the papers are on screen. The
    // mode and the object's active state are managed values, so they are read on
    // the script thread and mirrored here
    const bool open = snapshot.available.load(std::memory_order_relaxed) && snapshot.active.load(std::memory_order_relaxed);
    const int mode = snapshot.mode.load(std::memory_order_relaxed);

    if (!open || mode != static_cast<int>(Mode::ViewPapers)) {
        const bool viewCar = open && mode == static_cast<int>(Mode::ViewCar);
        Shared::RenderLabel(viewCar ? "LEAVE VIEW CAR" : "AUCTION NOT LOADED");
        ImGui::EndTabItem();
        return;
    }

    // A refresh tears the listing down and rebuilds it, so the button is only
    // usable once the game has finished spawning the new cars. This also blocks
    // the button during the initial load of the auction
    const bool refreshing = snapshot.refreshing.load(std::memory_order_relaxed);

    ImGui::BeginDisabled(refreshing);
    if (ImGui::Button("Refresh Auction")) {
        RefreshAuction();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Unlock All Cars")) {
        UnlockAllAuctionCars();
    }

    ImGui::EndTabItem();
}
