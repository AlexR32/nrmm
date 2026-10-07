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
        // The game's waitForCarSpawning flag: set while getNewCarsForSale or
        // loadCarsForSale is rebuilding the listing, cleared when it is done.
        // True means a load or refresh is still running
        std::atomic_bool refreshing{false};
    };

    static void RefreshAuction();
    static void UnlockAllAuctionCars();

    static Shared::Status status;
    static Snapshot snapshot;

    // Set on the script thread when the mod starts a refresh, cleared once
    // RefreshSnapshot sees the game has finished spawning the new cars. Only
    // used to report "Auction refreshed" for a mod-triggered refresh, not for
    // the auction's own initial load
    static bool refreshPending;
};
