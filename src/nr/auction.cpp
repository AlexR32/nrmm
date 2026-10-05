#include "pch.h"

#include "auction.h"
#include "shared.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"
#include "core/logger.h"

Shared::Status Auction::status("Auction");
Auction::Snapshot Auction::snapshot;

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
        int32_t lastRefresh = 0;
        Il2Cpp::SetInstanceFieldValue(carAuction, "lastRefresh", lastRefresh);

        Il2CppClass* auctionClass = Il2Cpp::FindClass("customization_carAuction");
        const MethodInfo* setup = Il2Cpp::GetMethod(auctionClass, "setupCarsForSale", 0);
        if (setup) {
            Il2Cpp::Invoke(setup, carAuction, nullptr);
            status.Set("Auction refreshed");
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

        std::vector<Il2Cpp::EnumMember> members = Il2Cpp::GetEnumMembers(chassisEnum);
        if (members.size() <= 2) { status.Set("No chassis values found"); return; }

        const size_t count = members.size() - 2;
        Il2CppArray* unlocked = Il2Cpp::NewArray(chassisEnum, count);
        if (!unlocked) { status.Set("Failed to allocate the unlocked array"); return; }

        for (size_t i = 0; i < count; ++i) {
            const Il2Cpp::EnumMember& member = members[i + 2];
            Il2Cpp::ArraySetRaw(unlocked, i, member.raw.data(), member.raw.size());
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

        status.Set("Unlocked " + std::to_string(count) + " cars");
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
        return;
    }

    // The auction GameObject can stay referenced after it has been switched
    // off, so the mode alone is not enough to tell whether it is really open
    snapshot.active.store(Il2Cpp::IsActiveSelf(carAuction), std::memory_order_relaxed);

    int32_t mode = -1;
    Il2Cpp::GetInstanceFieldValue(carAuction, "carAuctionMode", mode);
    snapshot.mode.store(mode, std::memory_order_relaxed);
}

void Auction::RenderTab() {
    if (!Shared::BeginGameTab("Auction")) return;

    status.Render();

    // The auction only accepts mod actions while the papers are on screen. The
    // mode and the object's active state are managed values, so they are read on
    // the script thread and mirrored here
    const bool open = snapshot.available.load(std::memory_order_relaxed)
        && snapshot.active.load(std::memory_order_relaxed);
    const int mode = snapshot.mode.load(std::memory_order_relaxed);

    if (!open || mode != static_cast<int>(Mode::ViewPapers)) {
        const bool viewCar = open && mode == static_cast<int>(Mode::ViewCar);
        Shared::RenderLabel(viewCar ? "LEAVE VIEW CAR" : "AUCTION NOT LOADED");
        ImGui::EndTabItem();
        return;
    }

    if (ImGui::Button("Refresh Auction")) {
        RefreshAuction();
    }

    ImGui::SameLine();
    if (ImGui::Button("Unlock All Cars")) {
        UnlockAllAuctionCars();
    }

    ImGui::EndTabItem();
}
