#pragma once
#include "pch.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"
#include "nr/shared.h"

// Auction tab: refreshes the car auction listing and unlocks every chassis for sale

class Auction {
public:
    static void RenderTab();
    static void RefreshSnapshot();

private:
    // customization_carAuction.CarAuctionMode
    enum class Mode : int32_t {
        IntroAnim = 0,  // _introAnim
        ViewPapers = 1, // _viewPapers
        ViewCar = 2,    // _viewCar
    };

    struct Snapshot {
        std::atomic_bool available{false};
        std::atomic_bool active{false};
        std::atomic<int> mode{-1};
    };

    static void RefreshAuction();
    static void UnlockAllAuctionCars();

    static Shared::Status status;
    static Snapshot snapshot;
};
