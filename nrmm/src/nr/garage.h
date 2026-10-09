#pragma once
#include "pch.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"
#include "nr/shared.h"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// Garage tab: mirrors the reference managed flow - find the CarParent and
// GodConstant instances, spawn a shop car, turn it into a working car and
// finally save it to savefile

class Garage {
public:
    static void RenderTab();
    static void Pump();

private:
    struct ModelOption {
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
        std::string modelName;
        std::vector<uint8_t> modelValue;
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

    static void LoadModelOptionsNow();
    static void LoadModelOptions(bool force = false);

    static void RenderOverrides();

    inline static Shared::Status status{"Garage"};
    inline static Shared::Sequence spawn;
    inline static SpawnContext ctx;

    inline static std::vector<ModelOption> modelOptions;
    inline static std::mutex modelMutex;
    // Bumped on the script thread whenever modelOptions is replaced; the render
    // thread re-copies the list only when the version changes instead of deep
    // copying every frame
    inline static std::atomic<uint32_t> modelOptionsVersion{0};
    inline static std::vector<ModelOption> modelOptionsView;
    inline static uint32_t modelOptionsViewVersion = 0;
    inline static int selectedModel = 0;
    inline static std::atomic_bool modelLoaded{false};
    inline static std::atomic_bool modelLoadAttempted{false};
    inline static std::atomic_bool modelLoadPending{false};
    inline static std::atomic_bool busy{false};

    inline static SpawnOverrides spawnOverrides;
};
