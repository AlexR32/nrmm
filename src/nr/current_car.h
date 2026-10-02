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

// Current car tab: the drivetrain, induction and fuel type, the freeze toggles
// and the oil/car-stats actions for the car the player is currently driving.
// The widgets read a small snapshot that RefreshSnapshot() rebuilds on the
// script thread from the GodConstant.Update hook, so the render thread never
// touches managed objects for display

class CurrentCar {
public:
    static void RenderTab();

    // Called from the GodConstant.Update hook, on the script thread
    static void RefreshSnapshot();

private:
    // Written by RefreshSnapshot() on the script thread, read by the render
    // thread. Each field is atomic so the widgets never race the writer
    struct Snapshot {
        std::atomic_bool carLoaded{false};

        std::atomic<int> drivetrainIndex{-1};
        std::atomic<int> inductionTypeIndex{-1};
        std::atomic<int> fuelTypeIndex{-1};

        std::atomic<float> engineHealth{0.0f};
        std::atomic<float> fuelTank{0.0f};
        std::atomic<float> fuelTankCapacity{0.0f};
        std::atomic<float> engineHeatWater{0.0f};
        std::atomic<float> engineHeatOil{0.0f};
        std::atomic<float> brakeTemp{0.0f};
        std::atomic<float> nosFuelLevel{0.0f};
    };

    static void ApplyDrivetrain(int index);
    static void ApplyInductionType(int index);
    static void ApplyFuelType(int index);

    static void ChangeOil();

    static void SaveCarStats();

    static Shared::Status status;
    static Snapshot snapshot;

    // Freeze toggles. RefreshSnapshot (script thread) holds the frozen values
    // each frame; the render thread writes the targets from the input widgets.
    // While a freeze is off its target tracks the live value, so enabling it
    // captures whatever the car currently has
    static std::atomic_bool freezeEngineHealth;
    static std::atomic_bool freezeFuel;
    static std::atomic_bool freezeWaterTemp;
    static std::atomic_bool freezeOilTemp;
    static std::atomic_bool freezeBrakeTemp;
    static std::atomic_bool freezeNos;
    static std::atomic<float> frozenEngineHealth;
    static std::atomic<float> frozenFuel;
    static std::atomic<float> frozenWaterTemp;
    static std::atomic<float> frozenOilTemp;
    static std::atomic<float> frozenBrakeTemp;
    static std::atomic<float> frozenNos;

    // Game constants
    static constexpr float kEngineHeatWater = 50.0f;
    static constexpr float kEngineHeatOil = 55.0f;
};
