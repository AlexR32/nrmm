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

// Current car tab: the drivetrain, induction and fuel type, the number plate
// editor, the freeze toggles and the oil/clean/save actions for the car the
// player is currently driving

class CurrentCar {
public:
    static void RenderTab();
    static void RefreshSnapshot();

private:
    // The editable number plate digits. plateType plus the eight glyph indices
    // the game's car_numberPlate.numberPlateInfo carries
    struct PlateValues {
        std::atomic<int> type{0};
        std::atomic<int> kanjiLeft{0};
        std::atomic<int> kanjiTop{0};
        std::atomic<int> numberTop1{0};
        std::atomic<int> numberTop2{0};
        std::atomic<int> numberTop3{0};
        std::atomic<int> numberBig1{0};
        std::atomic<int> numberBig2{0};
        std::atomic<int> numberBig3{0};
        std::atomic<int> numberBig4{0};
    };

    struct Snapshot {
        std::atomic_bool carLoaded{false};

        std::atomic<int> drivetrainIndex{-1};
        std::atomic<int> inductionTypeIndex{-1};
        std::atomic<int> fuelTypeIndex{-1};

        std::atomic<float> engineHealth{0.0f};
        std::atomic<float> fuelTank{0.0f};
        std::atomic<float> fuelTankCapacity{0.0f};
        std::atomic<float> carOdometerTarget{0.0f};
        std::atomic<float> engineHeatWater{0.0f};
        std::atomic<float> engineHeatOil{0.0f};
        std::atomic<float> brakeTemp{0.0f};
        std::atomic<float> nosFuelLevel{0.0f};

        // Live plate read from CarLocalCustom.localPlateInfo.
        std::atomic_bool plateLoaded{false};
        PlateValues plate;
    };

    static void ApplyDrivetrain(int index);
    static void ApplyInductionType(int index);
    static void ApplyFuelType(int index);

    // Rewrites localPlateInfo from plateEdit and refreshes both plate meshes
    static void ApplyNumberPlate();
    // Builds a fresh random plate through car_numberPlate.generateNumberPlate
    static void RandomizeNumberPlate();
    // Runs on the script thread: stores the info and re-runs numberPlate_start
    // on CarLocalCustom.plateFront and plateRear
    static void ApplyPlateToCar(Il2CppObject* carLocal, Il2CppObject* plateInfo);

    static void ChangeOil();
    static void CleanCar();

    static void SaveCar();

    static float SyncFloatField(Il2CppObject* instance, const char* fieldName, std::atomic_bool& freezeFlag, std::atomic<float>& frozenValue);

    // Game constants
    static constexpr float kEngineHeatWater = 50.0f;
    static constexpr float kEngineHeatOil = 55.0f;

    inline static Shared::Status status{"CurrentCar"};
    inline static Snapshot snapshot;

    // Number plate edit buffer. Owned by the render thread; resynced from the
    // live snapshot while plateDirty is false, and read when an action is posted
    inline static PlateValues plateEdit;
    inline static std::atomic_bool plateDirty{false};

    // Freeze toggles. While a freeze is off its target tracks the live value,
    // so enabling it captures whatever the car currently has
    inline static std::atomic_bool freezeEngineHealth{false};
    inline static std::atomic_bool freezeFuel{false};
    inline static std::atomic_bool freezeWaterTemp{false};
    inline static std::atomic_bool freezeOilTemp{false};
    inline static std::atomic_bool freezeBrakeTemp{false};
    inline static std::atomic_bool freezeNos{false};
    inline static std::atomic_bool freezeOdometer{false};
    inline static std::atomic<float> frozenEngineHealth{0.0f};
    inline static std::atomic<float> frozenFuel{0.0f};
    inline static std::atomic<float> frozenWaterTemp{kEngineHeatWater};
    inline static std::atomic<float> frozenOilTemp{kEngineHeatOil};
    inline static std::atomic<float> frozenBrakeTemp{0.0f};
    inline static std::atomic<float> frozenNos{0.0f};
    inline static std::atomic<float> frozenOdometer{0.0f};
};
