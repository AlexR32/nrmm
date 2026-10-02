#pragma once
#include "pch.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"
#include "nr/shared.h"

// Auction tab: refreshes the car auction listing and unlocks every chassis for
// sale. Both actions touch managed list/array state, so they are handed to the
// script thread through MainThread::Post

class Auction {
public:
    static void RenderTab();

    // Called from the GodConstant.Update hook, on the script thread
    static void RefreshSnapshot();

private:
    // customization_carAuction.CarAuctionMode. Only _viewPapers lets the mod
    // touch the listing; the other states are transitional or show a car
    enum class Mode : int32_t {
        IntroAnim = 0,  // _introAnim
        ViewPapers = 1, // _viewPapers
        ViewCar = 2,    // _viewCar
    };

    // Written by RefreshSnapshot() on the script thread, read by the render
    // thread. The mode is -1 while the auction has not been entered
    struct Snapshot {
        std::atomic_bool available{false};
        std::atomic_bool active{false};
        std::atomic<int> mode{-1};
    };

    static Il2CppObject* HomeGarage();
    static Il2CppObject* CarAuction();

    static int ListCount(Il2CppObject* list);
    static Il2CppObject* ListGet(Il2CppObject* list, int index);
    static void ListClear(Il2CppObject* list);

    static void RefreshAuction();
    static void UnlockAllAuctionCars();

    static Shared::Status status;
    static Snapshot snapshot;
};
