#include "pch.h"

#include "current_car.h"
#include "enums.h"
#include "shared.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"

Shared::Status CurrentCar::status("CurrentCar");
CurrentCar::Snapshot CurrentCar::snapshot;
CurrentCar::PlateValues CurrentCar::plateEdit;
std::atomic_bool CurrentCar::plateDirty{false};

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

void CurrentCar::CleanCar() {
    MainThread::Post([]() {
        Il2CppObject* carLocal = Shared::CarLocal();
        Il2CppObject* carData = Shared::CarData();
        if (!carLocal || !carData) { status.Set("Car not found"); return; }

        float carOdometerTarget = 0.0f;
        Il2Cpp::GetInstanceFieldValue(carData, "carOdometerTarget", carOdometerTarget);

        if (!Il2Cpp::SetInstanceFieldValue(carLocal, "car_dirtStartMilage", carOdometerTarget)) {
            status.Set("car_dirtStartMilage field not found");
            return;
        }

        Il2CppClass* carLocalClass = Il2Cpp::FindClass("CarLocalCustom");
        const MethodInfo* updateDirt = carLocalClass ? Il2Cpp::GetMethod(carLocalClass, "update_carDirt", 0) : nullptr;
        if (updateDirt) Il2Cpp::Invoke(updateDirt, carLocal, nullptr);

        status.Set("Car cleaned");
    });
}

// Number plate

void CurrentCar::ApplyPlateToCar(Il2CppObject* carLocal, Il2CppObject* plateInfo) {
    if (!carLocal || !plateInfo) return;

    Il2Cpp::SetInstanceFieldObject(carLocal, "localPlateInfo", plateInfo);

    Il2CppClass* plateClass = Il2Cpp::FindClass("car_numberPlate");
    const MethodInfo* start = plateClass ? Il2Cpp::GetMethod(plateClass, "numberPlate_start", 3) : nullptr;
    if (!start) { status.Set("numberPlate_start not found"); return; }

    const char* plateFields[2] = {"plateFront", "plateRear"};
    for (const char* fieldName : plateFields) {
        Il2CppObject* plate = Il2Cpp::GetInstanceFieldObject(carLocal, fieldName);
        if (!plate) continue;

        bool destroy = false;
        void* args[3] = {plateInfo, carLocal, &destroy};
        Il2Cpp::Invoke(start, plate, args);
    }
}

void CurrentCar::ApplyNumberPlate() {
    const int typeIndex = plateEdit.type.load(std::memory_order_relaxed);
    const int kanjiLeft = plateEdit.kanjiLeft.load(std::memory_order_relaxed);
    const int kanjiTop = plateEdit.kanjiTop.load(std::memory_order_relaxed);
    const int top1 = plateEdit.numberTop1.load(std::memory_order_relaxed);
    const int top2 = plateEdit.numberTop2.load(std::memory_order_relaxed);
    const int top3 = plateEdit.numberTop3.load(std::memory_order_relaxed);
    const int big1 = plateEdit.numberBig1.load(std::memory_order_relaxed);
    const int big2 = plateEdit.numberBig2.load(std::memory_order_relaxed);
    const int big3 = plateEdit.numberBig3.load(std::memory_order_relaxed);
    const int big4 = plateEdit.numberBig4.load(std::memory_order_relaxed);

    MainThread::Post([=]() {
        Il2CppObject* carLocal = Shared::CarLocal();
        if (!carLocal) { status.Set("CarLocal not found"); plateDirty.store(false, std::memory_order_relaxed); return; }

        const std::vector<Il2Cpp::EnumMember>& types = Enums::PlateTypes();
        if (typeIndex < 0 || typeIndex >= static_cast<int>(types.size())) {
            status.Set("Plate types unavailable");
            plateDirty.store(false, std::memory_order_relaxed);
            return;
        }

        Il2CppObject* plateInfo = Il2Cpp::GetInstanceFieldObject(carLocal, "localPlateInfo");
        if (!plateInfo) {
            // No stored plate yet: generate one so there is an object to edit
            Il2CppClass* plateClass = Il2Cpp::FindClass("car_numberPlate");
            const MethodInfo* generate = plateClass ? Il2Cpp::GetMethod(plateClass, "generateNumberPlate", 1) : nullptr;
            if (!generate) { status.Set("generateNumberPlate not found"); plateDirty.store(false, std::memory_order_relaxed); return; }
            int32_t plateType = 0;
            void* genArgs[1] = {&plateType};
            plateInfo = Il2Cpp::Invoke(generate, nullptr, genArgs);
        }
        if (!plateInfo) { status.Set("Failed to create plate info"); plateDirty.store(false, std::memory_order_relaxed); return; }

        if (!Shared::SetEnumField(plateInfo, "plateType", types[typeIndex])) {
            status.Set("plateType field not found");
            plateDirty.store(false, std::memory_order_relaxed);
            return;
        }

        const int32_t kanjiLeftValue = kanjiLeft;
        const int32_t kanjiTopValue = kanjiTop;
        const int32_t top1Value = top1;
        const int32_t top2Value = top2;
        const int32_t top3Value = top3;
        const int32_t big1Value = big1;
        const int32_t big2Value = big2;
        const int32_t big3Value = big3;
        const int32_t big4Value = big4;
        Il2Cpp::SetInstanceFieldValue(plateInfo, "kanji_left", kanjiLeftValue);
        Il2Cpp::SetInstanceFieldValue(plateInfo, "kanji_top", kanjiTopValue);
        Il2Cpp::SetInstanceFieldValue(plateInfo, "number_top_1", top1Value);
        Il2Cpp::SetInstanceFieldValue(plateInfo, "number_top_2", top2Value);
        Il2Cpp::SetInstanceFieldValue(plateInfo, "number_top_3", top3Value);
        Il2Cpp::SetInstanceFieldValue(plateInfo, "number_big_1", big1Value);
        Il2Cpp::SetInstanceFieldValue(plateInfo, "number_big_2", big2Value);
        Il2Cpp::SetInstanceFieldValue(plateInfo, "number_big_3", big3Value);
        Il2Cpp::SetInstanceFieldValue(plateInfo, "number_big_4", big4Value);

        ApplyPlateToCar(carLocal, plateInfo);
        status.Set("Number plate updated");
        plateDirty.store(false, std::memory_order_relaxed);
    });
}

void CurrentCar::RandomizeNumberPlate() {
    const int typeIndex = plateEdit.type.load(std::memory_order_relaxed);

    MainThread::Post([typeIndex]() {
        Il2CppObject* carLocal = Shared::CarLocal();
        if (!carLocal) { status.Set("CarLocal not found"); plateDirty.store(false, std::memory_order_relaxed); return; }

        Il2CppClass* plateClass = Il2Cpp::FindClass("car_numberPlate");
        const MethodInfo* generate = plateClass ? Il2Cpp::GetMethod(plateClass, "generateNumberPlate", 1) : nullptr;
        if (!generate) { status.Set("generateNumberPlate not found"); plateDirty.store(false, std::memory_order_relaxed); return; }

        // NumberPlateType black/green/glow map to 0/1/2, so the combo index is
        // the enum value the generator expects
        int32_t plateType = typeIndex < 0 ? 0 : typeIndex;
        void* genArgs[1] = {&plateType};
        Il2CppObject* plateInfo = Il2Cpp::Invoke(generate, nullptr, genArgs);
        if (!plateInfo) { status.Set("generateNumberPlate returned null"); plateDirty.store(false, std::memory_order_relaxed); return; }

        ApplyPlateToCar(carLocal, plateInfo);
        status.Set("Number plate randomized");
        plateDirty.store(false, std::memory_order_relaxed);
    });
}

// Car save
void CurrentCar::SaveCar() {
    MainThread::Post([]() {
        Il2CppClass* godClass = Il2Cpp::FindClass("GodConstant");
        Il2CppObject* god = Shared::God();
        Il2CppObject* carLocal = Shared::CarLocal();
        if (!godClass || !god || !carLocal) { status.Set("Car or GodConstant not found"); return; }

        const MethodInfo* saveEngine = Il2Cpp::GetMethod(godClass, "saveCarEngine", 1);
        const MethodInfo* saveParts = Il2Cpp::GetMethod(godClass, "saveCarParts", 3);
        const MethodInfo* updateStats = Il2Cpp::GetMethod(godClass, "player_updateCarCacheStats", 1);
        if (!saveEngine || !saveParts || !updateStats) { status.Set("Car save methods not found"); return; }

        // saveCarEngine(carlocal)
        void* engineArgs[1] = {carLocal};
        Il2Cpp::Invoke(saveEngine, god, engineArgs);

        // saveCarParts(carlocal, GodConstant.SaveParts._all, false)
        int32_t mode = 7; // SaveParts._all
        bool firstTimeSave = false;
        void* partsArgs[3] = {carLocal, &mode, &firstTimeSave};
        Il2Cpp::Invoke(saveParts, god, partsArgs);

        // player_updateCarCacheStats(true)
        bool save = true;
        void* statsArgs[1] = {&save};
        Il2Cpp::Invoke(updateStats, god, statsArgs);

        status.Set("Car saved");
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

    // Number plate
    snapshot.plateLoaded.store(carLocal != nullptr, std::memory_order_relaxed);
    Il2CppObject* plateInfo = carLocal ? Il2Cpp::GetInstanceFieldObject(carLocal, "localPlateInfo") : nullptr;
    if (plateInfo) {
        int32_t plateType = 0, kanjiLeft = 0, kanjiTop = 0;
        int32_t top1 = 0, top2 = 0, top3 = 0;
        int32_t big1 = 0, big2 = 0, big3 = 0, big4 = 0;
        Il2Cpp::GetInstanceFieldValue(plateInfo, "plateType", plateType);
        Il2Cpp::GetInstanceFieldValue(plateInfo, "kanji_left", kanjiLeft);
        Il2Cpp::GetInstanceFieldValue(plateInfo, "kanji_top", kanjiTop);
        Il2Cpp::GetInstanceFieldValue(plateInfo, "number_top_1", top1);
        Il2Cpp::GetInstanceFieldValue(plateInfo, "number_top_2", top2);
        Il2Cpp::GetInstanceFieldValue(plateInfo, "number_top_3", top3);
        Il2Cpp::GetInstanceFieldValue(plateInfo, "number_big_1", big1);
        Il2Cpp::GetInstanceFieldValue(plateInfo, "number_big_2", big2);
        Il2Cpp::GetInstanceFieldValue(plateInfo, "number_big_3", big3);
        Il2Cpp::GetInstanceFieldValue(plateInfo, "number_big_4", big4);

        snapshot.plate.type.store(plateType, std::memory_order_relaxed);
        snapshot.plate.kanjiLeft.store(kanjiLeft, std::memory_order_relaxed);
        snapshot.plate.kanjiTop.store(kanjiTop, std::memory_order_relaxed);
        snapshot.plate.numberTop1.store(top1, std::memory_order_relaxed);
        snapshot.plate.numberTop2.store(top2, std::memory_order_relaxed);
        snapshot.plate.numberTop3.store(top3, std::memory_order_relaxed);
        snapshot.plate.numberBig1.store(big1, std::memory_order_relaxed);
        snapshot.plate.numberBig2.store(big2, std::memory_order_relaxed);
        snapshot.plate.numberBig3.store(big3, std::memory_order_relaxed);
        snapshot.plate.numberBig4.store(big4, std::memory_order_relaxed);
    }
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
    ImGui::SameLine();
    if (ImGui::Button("Clean Car")) {
        CleanCar();
    }
    
    if (ImGui::Button("Save Car")) {
        SaveCar();
    }

    if (snapshot.plateLoaded.load(std::memory_order_relaxed)) {
        ImGui::SeparatorText("Number plate");

        if (!plateDirty.load(std::memory_order_relaxed)) {
            plateEdit.type.store(snapshot.plate.type.load(std::memory_order_relaxed), std::memory_order_relaxed);
            plateEdit.kanjiLeft.store(snapshot.plate.kanjiLeft.load(std::memory_order_relaxed), std::memory_order_relaxed);
            plateEdit.kanjiTop.store(snapshot.plate.kanjiTop.load(std::memory_order_relaxed), std::memory_order_relaxed);
            plateEdit.numberTop1.store(snapshot.plate.numberTop1.load(std::memory_order_relaxed), std::memory_order_relaxed);
            plateEdit.numberTop2.store(snapshot.plate.numberTop2.load(std::memory_order_relaxed), std::memory_order_relaxed);
            plateEdit.numberTop3.store(snapshot.plate.numberTop3.load(std::memory_order_relaxed), std::memory_order_relaxed);
            plateEdit.numberBig1.store(snapshot.plate.numberBig1.load(std::memory_order_relaxed), std::memory_order_relaxed);
            plateEdit.numberBig2.store(snapshot.plate.numberBig2.load(std::memory_order_relaxed), std::memory_order_relaxed);
            plateEdit.numberBig3.store(snapshot.plate.numberBig3.load(std::memory_order_relaxed), std::memory_order_relaxed);
            plateEdit.numberBig4.store(snapshot.plate.numberBig4.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }

        if (!Enums::Ready() || Enums::PlateTypes().empty()) {
            ImGui::TextUnformatted("Plate types unavailable.");
        } else {
            Shared::RenderEnumCombo("Plate type", Enums::PlateTypes(), plateEdit.type.load(std::memory_order_relaxed), [](int index) {
                plateEdit.type.store(index, std::memory_order_relaxed);
                plateDirty.store(true, std::memory_order_relaxed);
            });
        }

        // The game's plate digit sprites: kanji_left 0..9, kanji_top 0..10,
        // the top serial digits 0..8 and the big serial digits 0..9
        auto renderPlateField = [](const char* label, std::atomic<int>& field, int minValue, int maxValue) {
            int value = field.load(std::memory_order_relaxed);
            ImGui::SetNextItemWidth(45.0f);
            if (ImGui::InputInt(label, &value, 0, 0)) {
                if (value < minValue) value = minValue;
                if (value > maxValue) value = maxValue;
                if (field.load(std::memory_order_relaxed) != value) {
                    field.store(value, std::memory_order_relaxed);
                    plateDirty.store(true, std::memory_order_relaxed);
                }
            }
        };

        renderPlateField("Kanji left", plateEdit.kanjiLeft, 0, 9);
        ImGui::SameLine();
        renderPlateField("Kanji top", plateEdit.kanjiTop, 0, 10);

        renderPlateField("Top 1", plateEdit.numberTop1, 0, 8);
        ImGui::SameLine();
        renderPlateField("Top 2", plateEdit.numberTop2, 0, 8);
        ImGui::SameLine();
        renderPlateField("Top 3", plateEdit.numberTop3, 0, 8);

        renderPlateField("Big 1", plateEdit.numberBig1, 0, 9);
        ImGui::SameLine();
        renderPlateField("Big 2", plateEdit.numberBig2, 0, 9);
        ImGui::SameLine();
        renderPlateField("Big 3", plateEdit.numberBig3, 0, 9);
        ImGui::SameLine();
        renderPlateField("Big 4", plateEdit.numberBig4, 0, 9);

        if (ImGui::Button("Apply plate")) {
            ApplyNumberPlate();
        }
        ImGui::SameLine();
        if (ImGui::Button("Random plate")) {
            RandomizeNumberPlate();
        }
    }

    ImGui::EndTabItem();
}
