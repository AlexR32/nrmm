#pragma once
#include "pch.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"
#include "nr/shared.h"
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

// Garage tab: mirrors the reference managed flow - find the CarParent and
// GodConstant instances, spawn a shop car, turn it into a working car and
// finally save it to savefile.
//
// Unity drives coroutines from its player loop, so the sequence is advanced one
// step per frame by PumpSpawn from the GodConstant.Update hook instead of
// running on a worker thread.

class Garage {
public:
    static void RenderTab();

    // Advanced from the GodConstant.Update hook
    // so the sequence runs on the game's script thread
    static void PumpSpawn();

private:
    struct ChassisOption {
        std::string name;
        std::vector<uint8_t> raw;
    };

    struct SpawnOverrides {
        bool applyToggle = false;

        bool engineToggle = false;
        float engine = 100.0f;

        bool fuelToggle = false;
        float fuel = 100.0f;

        bool mileageToggle = false;
        float mileage = 0.0f;
        float dirtMileage = 0.0f;
        float mileageSinceLastOilChange = 0.0f;

        bool gearboxToggle = false;
        bool gearbox = false;

        bool paintToggle = false;
        float paint[3] = {1.0f, 1.0f, 1.0f};

        bool onlyStock = true;
    };

    struct SpawnContext {
        std::string chassisName;
        std::vector<uint8_t> chassisValue;
        std::vector<uint8_t> setupValue;
        SpawnOverrides overrides;

        Il2CppClass* carLocalClass = nullptr;
        const MethodInfo* shopCarSpawnMethod = nullptr;
        const MethodInfo* saveCarMethod = nullptr;

        Il2CppObject* parent = nullptr;
        Il2CppObject* god = nullptr;
        Il2CppObject* targetCar = nullptr;
        Il2CppObject* spawnedObject = nullptr;

        bool isStillCar = true;
        bool engineRunning = false;
        bool saveToHouse = false;
    };

    static bool SetCarField(Il2CppObject* targetCar, const char* fieldName, float value);
    static void ApplyPaint(Il2CppObject* targetCar, const SpawnOverrides& overrides);
    static void ApplyOverrides(Il2CppObject* targetCar, const SpawnOverrides& overrides);

    static void StartSpawn();
    static void AddSelectedToGarage();

    static void LoadChassisOptionsNow();
    static void LoadChassisOptions(bool force = false);

    static void RenderOverrides();

    static Shared::Status status;
    static Shared::Sequence spawn;
    static SpawnContext ctx;

    static std::vector<ChassisOption> chassisOptions;
    static int selectedChassis;
    static std::atomic_bool chassisLoaded;
    static std::atomic_bool chassisLoadAttempted;
    static std::atomic_bool chassisLoadPending;
    static std::atomic_bool busy;

    static SpawnOverrides spawnOverrides;
};
