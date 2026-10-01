#include "pch.h"

#include "current_car.h"
#include "enums.h"
#include "shared.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"

Shared::Status CurrentCar::status("CurrentCar");
CurrentCar::Snapshot CurrentCar::snapshot;

std::atomic_bool CurrentCar::freezeEngineHealth{false};
std::atomic_bool CurrentCar::freezeFuel{false};
std::atomic_bool CurrentCar::freezeWaterTemp{false};
std::atomic_bool CurrentCar::freezeOilTemp{false};
std::atomic_bool CurrentCar::freezeBrakeTemp{false};
std::atomic_bool CurrentCar::freezeNos{false};
std::atomic<float> CurrentCar::frozenEngineHealth{0.0f};
std::atomic<float> CurrentCar::frozenFuel{0.0f};
std::atomic<float> CurrentCar::frozenWaterTemp{kEngineHeatWater};
std::atomic<float> CurrentCar::frozenOilTemp{kEngineHeatOil};
std::atomic<float> CurrentCar::frozenBrakeTemp{0.0f};
std::atomic<float> CurrentCar::frozenNos{0.0f};

// Apply actions

void CurrentCar::ApplyDrivetrain(int index) {
    MainThread::Post([index]() {
        Il2CppObject* carData = Shared::CarData();
        const std::vector<Il2Cpp::EnumMember>& drivetrains = Enums::DrivetrainTypes();
        if (!carData || index < 0 || index >= static_cast<int>(drivetrains.size())) return;

        if (!Shared::SetEnumField(carData, "drivetrain", drivetrains[index])) {
            status.Set("drivetrain field not found");
            return;
        }
        status.Set("Drivetrain set to " + drivetrains[index].name);
    });
}

void CurrentCar::ApplyInductionType(int index) {
    MainThread::Post([index]() {
        Il2CppObject* carData = Shared::CarData();
        const std::vector<Il2Cpp::EnumMember>& inductionTypes = Enums::InductionTypes();
        if (!carData || index < 0 || index >= static_cast<int>(inductionTypes.size())) return;

        if (!Shared::SetEnumField(carData, "inductionType", inductionTypes[index])) {
            status.Set("inductionType field not found");
            return;
        }
        status.Set("Induction type set to " + inductionTypes[index].name);
    });
}

void CurrentCar::ApplyFuelType(int index) {
    MainThread::Post([index]() {
        Il2CppObject* carLocal = Shared::CarLocal();
        const std::vector<Il2Cpp::EnumMember>& fuelTypes = Enums::FuelTypes();
        if (!carLocal || index < 0 || index >= static_cast<int>(fuelTypes.size())) return;

        if (!Shared::SetEnumField(carLocal, "current_fuelType", fuelTypes[index])) {
            status.Set("current_fuelType field not found");
            return;
        }
        status.Set("Fuel type set to " + fuelTypes[index].name);
    });
}

void CurrentCar::ChangeOil() {
    MainThread::Post([]() {
        Il2CppObject* carData = Shared::CarData();
        if (!carData) { status.Set("CarData not found"); return; }

        float carOdometerTarget = 0.0f;
        Il2Cpp::GetInstanceFieldValue(carData, "carOdometerTarget", carOdometerTarget);
        Il2Cpp::SetInstanceFieldValue(carData, "odometerAtLastOilChange", carOdometerTarget);
        status.Set("Oil changed successfully");
    });
}

// Car stats save

void CurrentCar::SaveCarStats() {
    MainThread::Post([]() {
        Il2CppClass* godClass = Il2Cpp::FindClass("GodConstant");
        Il2CppObject* god = Shared::God();
        if (!godClass || !god) { status.Set("GodConstant not found"); return; }

        const MethodInfo* method = Il2Cpp::GetMethod(godClass, "player_updateCarCacheStats", 1);
        if (!method) { status.Set("player_updateCarCacheStats not found"); return; }

        bool save = true;
        void* args[1] = {&save};
        Il2Cpp::Invoke(method, god, args);
        status.Set("Car stats saved");
    });
}

// Snapshot / tab

void CurrentCar::RefreshSnapshot() {
    Il2Cpp::ThreadAttach();

    if (Enums::Ready()) {
        snapshot.drivetrainIndex.store(Enums::SyncIndex(Enums::Id::Drivetrain, Shared::CarData(), "drivetrain"), std::memory_order_relaxed);
        snapshot.inductionTypeIndex.store(Enums::SyncIndex(Enums::Id::InductionType, Shared::CarData(), "inductionType"), std::memory_order_relaxed);
        snapshot.fuelTypeIndex.store(Enums::SyncIndex(Enums::Id::FuelType, Shared::CarLocal(), "current_fuelType"), std::memory_order_relaxed);
    }

    const bool freezeHealth = freezeEngineHealth.load(std::memory_order_relaxed);
    const bool freezeFuelOn = freezeFuel.load(std::memory_order_relaxed);
    const bool freezeWater = freezeWaterTemp.load(std::memory_order_relaxed);
    const bool freezeOil = freezeOilTemp.load(std::memory_order_relaxed);
    const bool freezeBrake = freezeBrakeTemp.load(std::memory_order_relaxed);
    const bool nosFreeze = freezeNos.load(std::memory_order_relaxed);

    float engineHealth = 0.0f;
    Il2CppObject* carEngine = Shared::CarEngine();
    const bool haveEngineHealth = carEngine && Il2Cpp::GetInstanceFieldValue(carEngine, "engineHealth", engineHealth);

    float fuelTank = 0.0f;
    float fuelTankCapacity = 0.0f;

    Il2CppObject* car = Shared::PlayerCar();
    snapshot.carLoaded.store(car != nullptr, std::memory_order_relaxed);

    Il2Cpp::GetInstanceFieldValue(car, "fuelTank", fuelTank);
    Il2Cpp::GetInstanceFieldValue(car, "fuelTankCapacity", fuelTankCapacity);

    float water = 0.0f;
    float oil = 0.0f;
    float brakeTemp = 0.0f;
    if (car) {
        Il2Cpp::GetInstanceFieldValue(car, "engineHeatWater", water);
        Il2Cpp::GetInstanceFieldValue(car, "engineHeatOil", oil);
    }

    Il2CppObject* carLocal = Shared::CarLocal();
    if (carLocal) {
        Il2Cpp::GetInstanceFieldValue(carLocal, "brakeTemp", brakeTemp);
    }

    // Engine health: hold the menu target, or track the live value so enabling
    // the freeze captures whatever the engine currently has
    if (freezeHealth) {
        engineHealth = frozenEngineHealth.load(std::memory_order_relaxed);
        if (carEngine) Il2Cpp::SetInstanceFieldValue(carEngine, "engineHealth", engineHealth);
    } else if (haveEngineHealth) {
        frozenEngineHealth.store(engineHealth, std::memory_order_relaxed);
    }

    // Fuel
    if (freezeFuelOn) {
        float heldFuel = frozenFuel.load(std::memory_order_relaxed);
        if (fuelTankCapacity > 0.0f && heldFuel > fuelTankCapacity) heldFuel = fuelTankCapacity;
        fuelTank = heldFuel;
        if (car) Il2Cpp::SetInstanceFieldValue(car, "fuelTank", fuelTank);
    } else {
        frozenFuel.store(fuelTank, std::memory_order_relaxed);
    }

    // Water heat
    if (freezeWater) {
        water = frozenWaterTemp.load(std::memory_order_relaxed);
        if (car) Il2Cpp::SetInstanceFieldValue(car, "engineHeatWater", water);
    } else {
        frozenWaterTemp.store(water, std::memory_order_relaxed);
    }
    // Oil heat
    if (freezeOil) {
        oil = frozenOilTemp.load(std::memory_order_relaxed);
        if (car) Il2Cpp::SetInstanceFieldValue(car, "engineHeatOil", oil);
    } else {
        frozenOilTemp.store(oil, std::memory_order_relaxed);
    }
    // Brake heat
    if (freezeBrake) {
        brakeTemp = frozenBrakeTemp.load(std::memory_order_relaxed);
        if (carLocal) {
            // Brake fade is always pinned to zero so the brakes never go soft.
            const float zero = 0.0f;
            Il2Cpp::SetInstanceFieldValue(carLocal, "brakeTemp", brakeTemp);
            Il2Cpp::SetInstanceFieldValue(carLocal, "brakeFade_temp", zero);
        }
    } else {
        frozenBrakeTemp.store(brakeTemp, std::memory_order_relaxed);
    }

    if (haveEngineHealth || freezeHealth) snapshot.engineHealth.store(engineHealth, std::memory_order_relaxed);
    snapshot.fuelTank.store(fuelTank, std::memory_order_relaxed);
    snapshot.fuelTankCapacity.store(fuelTankCapacity, std::memory_order_relaxed);
    snapshot.engineHeatWater.store(water, std::memory_order_relaxed);
    snapshot.engineHeatOil.store(oil, std::memory_order_relaxed);
    snapshot.brakeTemp.store(brakeTemp, std::memory_order_relaxed);

    // NOS
    float nos = 0.0f;
    Il2CppObject* carData = Shared::CarData();
    const bool haveNos = carData && Il2Cpp::GetInstanceFieldValue(carData, "NOS_fuelLevel", nos);
    if (nosFreeze) {
        nos = frozenNos.load(std::memory_order_relaxed);
        if (carData) Il2Cpp::SetInstanceFieldValue(carData, "NOS_fuelLevel", nos);
    } else if (haveNos) {
        frozenNos.store(nos, std::memory_order_relaxed);
    }
    if (haveNos || nosFreeze) snapshot.nosFuelLevel.store(nos, std::memory_order_relaxed);
}

void CurrentCar::RenderTab() {
    if (!Shared::BeginGameTab("Current Car")) return;

    const bool carLoaded = snapshot.carLoaded.load(std::memory_order_relaxed);

    if (!carLoaded) {
        Shared::RenderLabel("CAR NOT LOADED");
        ImGui::EndTabItem();
        return;
    }

    status.Render();

    // Drivetrain types
    if (!Enums::Ready() || Enums::DrivetrainTypes().empty()) {
        ImGui::TextUnformatted("Drivetrain types unavailable.");
    } else {
        Shared::RenderEnumCombo("Drivetrain type", Enums::DrivetrainTypes(), snapshot.drivetrainIndex.load(std::memory_order_relaxed), ApplyDrivetrain);
    }

    // Induction types
    if (!Enums::Ready() || Enums::InductionTypes().empty()) {
        ImGui::TextUnformatted("Induction types unavailable.");
    } else {
        Shared::RenderEnumCombo("Induction type", Enums::InductionTypes(), snapshot.inductionTypeIndex.load(std::memory_order_relaxed), ApplyInductionType);
    }

    // Fuel types
    if (!Enums::Ready() || Enums::FuelTypes().empty()) {
        ImGui::TextUnformatted("Fuel types unavailable.");
    } else {
        Shared::RenderEnumCombo("Fuel type", Enums::FuelTypes(), snapshot.fuelTypeIndex.load(std::memory_order_relaxed), ApplyFuelType);
    }


    // Freeze checkbox lambda for ease of use
    void (*renderFreeze)(const char* id, bool value, std::atomic_bool & target) = [](const char* id, bool value, std::atomic_bool& target) {
        bool toggled = value;
        if (ImGui::Checkbox(id, &toggled)) target.store(toggled, std::memory_order_relaxed);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Freeze value");
    };

    // Engine health
    const bool freezeHealthOn = freezeEngineHealth.load(std::memory_order_relaxed);
    renderFreeze("##freezeEngineHealth", freezeHealthOn, freezeEngineHealth);
    ImGui::SameLine();

    float engineHealth = freezeHealthOn ? frozenEngineHealth.load(std::memory_order_relaxed) : snapshot.engineHealth.load(std::memory_order_relaxed);
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputFloat("Engine Health", &engineHealth, 1.0f, 10.0f, "%.1f")) {
        if (engineHealth > 100.0f) engineHealth = 100.0f;
        if (engineHealth < 0.0f) engineHealth = 0.0f;
        frozenEngineHealth.store(engineHealth, std::memory_order_relaxed);
        const float value = engineHealth;
        // Re-resolve the engine on the script thread: a cached pointer can go
        // stale between frames when the garbage collector moves things.
        MainThread::Post([value]() {
            Il2CppObject* carEngine = Shared::CarEngine();
            if (carEngine) Il2Cpp::SetInstanceFieldValue(carEngine, "engineHealth", value);
        });
    }

    // Fuel
    const bool freezeFuelOn = freezeFuel.load(std::memory_order_relaxed);
    renderFreeze("##freezeFuel", freezeFuelOn, freezeFuel);
    ImGui::SameLine();

    const float fuelTankCapacity = snapshot.fuelTankCapacity.load(std::memory_order_relaxed);
    float fuelTank = freezeFuelOn ? frozenFuel.load(std::memory_order_relaxed) : snapshot.fuelTank.load(std::memory_order_relaxed);
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputFloat("Fuel Tank", &fuelTank, 1.0f, 10.0f, "%.1f")) {
        if (fuelTank > fuelTankCapacity) fuelTank = fuelTankCapacity;
        if (fuelTank < 0.0f) fuelTank = 0.0f;
        frozenFuel.store(fuelTank, std::memory_order_relaxed);
        const float value = fuelTank;
        MainThread::Post([value]() {
            Il2CppObject* car = Shared::PlayerCar();
            if (car) Il2Cpp::SetInstanceFieldValue(car, "fuelTank", value);
        });
    }

    // Water heat
    const bool freezeWaterOn = freezeWaterTemp.load(std::memory_order_relaxed);
    renderFreeze("##freezeWater", freezeWaterOn, freezeWaterTemp);
    ImGui::SameLine();

    float waterTemp = freezeWaterOn ? frozenWaterTemp.load(std::memory_order_relaxed) : snapshot.engineHeatWater.load(std::memory_order_relaxed);
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputFloat("Water Temp", &waterTemp, 1.0f, 10.0f, "%.1f")) {
        if (waterTemp > 140.0f) waterTemp = 140.0f;
        if (waterTemp < 30.0f) waterTemp = 30.0f;
        frozenWaterTemp.store(waterTemp, std::memory_order_relaxed);
        const float value = waterTemp;
        MainThread::Post([value]() {
            Il2CppObject* car = Shared::PlayerCar();
            if (car) Il2Cpp::SetInstanceFieldValue(car, "engineHeatWater", value);
        });
    }

    // Oil heat
    const bool freezeOilOn = freezeOilTemp.load(std::memory_order_relaxed);
    renderFreeze("##freezeOil", freezeOilOn, freezeOilTemp);
    ImGui::SameLine();

    float oilTemp = freezeOilOn ? frozenOilTemp.load(std::memory_order_relaxed) : snapshot.engineHeatOil.load(std::memory_order_relaxed);
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputFloat("Oil Temp", &oilTemp, 1.0f, 10.0f, "%.1f")) {
        if (oilTemp > 140.0f) oilTemp = 140.0f;
        if (oilTemp < 30.0f) oilTemp = 30.0f;
        frozenOilTemp.store(oilTemp, std::memory_order_relaxed);
        const float value = oilTemp;
        MainThread::Post([value]() {
            Il2CppObject* car = Shared::PlayerCar();
            if (car) Il2Cpp::SetInstanceFieldValue(car, "engineHeatOil", value);
        });
    }

    // Brake heat
    const bool freezeBrakeOn = freezeBrakeTemp.load(std::memory_order_relaxed);
    renderFreeze("##freezeBrake", freezeBrakeOn, freezeBrakeTemp);
    ImGui::SameLine();

    float brakeTemp = freezeBrakeOn ? frozenBrakeTemp.load(std::memory_order_relaxed) : snapshot.brakeTemp.load(std::memory_order_relaxed);
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputFloat("Brake Temp", &brakeTemp, 0.01f, 0.1f, "%.2f")) {
        if (brakeTemp > 1.0f) brakeTemp = 1.0f;
        if (brakeTemp < 0.0f) brakeTemp = 0.0f;
        frozenBrakeTemp.store(brakeTemp, std::memory_order_relaxed);
        const float value = brakeTemp;
        MainThread::Post([value]() {
            Il2CppObject* carLocal = Shared::CarLocal();
            if (carLocal) Il2Cpp::SetInstanceFieldValue(carLocal, "brakeTemp", value);
        });
    }

    // NOS
    const bool freezeNosOn = freezeNos.load(std::memory_order_relaxed);
    renderFreeze("##freezeNos", freezeNosOn, freezeNos);
    ImGui::SameLine();

    float nos = freezeNosOn ? frozenNos.load(std::memory_order_relaxed) : snapshot.nosFuelLevel.load(std::memory_order_relaxed);
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputFloat("NOS", &nos, 0.01f, 0.1f, "%.2f")) {
        if (nos > 1.0f) nos = 1.0f;
        if (nos < 0.0f) nos = 0.0f;
        frozenNos.store(nos, std::memory_order_relaxed);
        const float value = nos;
        MainThread::Post([value]() {
            Il2CppObject* carData = Shared::CarData();
            if (carData) Il2Cpp::SetInstanceFieldValue(carData, "NOS_fuelLevel", value);
        });
    }

    if (ImGui::Button("Change Oil")) {
        ChangeOil();
    }
    if (ImGui::Button("Save Car Stats")) {
        SaveCarStats();
    }

    ImGui::EndTabItem();
}
