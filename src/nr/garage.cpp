#include "pch.h"

#include "garage.h"
#include "shared.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"
#include "core/logger.h"

Shared::Status Garage::status("Garage");
Shared::Sequence Garage::spawn;
Garage::SpawnContext Garage::ctx;

std::vector<Garage::ChassisOption> Garage::chassisOptions;
std::mutex Garage::chassisMutex;
int Garage::selectedChassis = 0;
std::atomic_bool Garage::chassisLoaded{ false };
std::atomic_bool Garage::chassisLoadAttempted{ false };
std::atomic_bool Garage::chassisLoadPending{ false };
std::atomic_bool Garage::busy{ false };

Garage::SpawnOverrides Garage::spawnOverrides;

// Helper to set car overrides
bool Garage::SetCarField(Il2CppObject* targetCar, const char* fieldName, float value) {
    if (Il2Cpp::SetInstanceFieldValue(targetCar, fieldName, value)) return true;
    Logger::Log(std::string("[Garage] Field not found: ") + fieldName);
    return false;
}

// Only the main paint slot is touched; rim and secondary stay untouched
void Garage::ApplyPaint(Il2CppObject* targetCar, const SpawnOverrides& overrides) {
    Il2CppObject* package = Il2Cpp::GetInstanceFieldObject(targetCar, "paint_package");
    if (!package) {
        package = Il2Cpp::NewObject(Il2Cpp::FindClass("Car_PaintPackage"));
        if (!package || !Il2Cpp::SetInstanceFieldObject(targetCar, "paint_package", package)) {
            Logger::Log("[Garage] Could not read or create paint_package");
            return;
        }
    }

    Il2CppObject* mainPaint = Il2Cpp::GetInstanceFieldObject(package, "carPaint_main");
    if (!mainPaint) {
        mainPaint = Il2Cpp::NewObject(Il2Cpp::FindClass("Car_Paint"));
        if (!mainPaint || !Il2Cpp::SetInstanceFieldObject(package, "carPaint_main", mainPaint)) {
            Logger::Log("[Garage] Could not read or create carPaint_main");
            return;
        }
    }

    Shared::Colour colour = {overrides.paint[0], overrides.paint[1], overrides.paint[2], 1.0f};
    if (!Il2Cpp::SetInstanceFieldValue(mainPaint, "main_colour", colour)) {
        Logger::Log("[Garage] Field not found: main_colour");
    }
}

void Garage::ApplyOverrides(Il2CppObject* targetCar, const SpawnOverrides& overrides) {
    if (!targetCar || !overrides.applyToggle) return;

    if (overrides.engineToggle) {
        SetCarField(targetCar, "engineHealth", overrides.engine);
    }

    if (overrides.fuelToggle) {
        SetCarField(targetCar, "carAuction_fuelTankAmount", overrides.fuel);
    }

    if (overrides.mileageToggle) {
        SetCarField(targetCar, "car_mileage", overrides.mileage);
        SetCarField(targetCar, "car_dirtMileage", overrides.dirtMileage);
        SetCarField(targetCar, "car_mileageSinceLastOilChange", overrides.mileageSinceLastOilChange);
    }

    if (overrides.gearboxToggle) {
        SetCarField(targetCar, "carAuction_autoGear", overrides.gearbox);
    }

    if (overrides.paintToggle) {
        ApplyPaint(targetCar, overrides);
    }
}

// Both spawnShopCar and ShopCarSpawn are coroutines, so they can only be driven
// by the game running its own player loop. The sequence is expressed as steps
// that PumpSpawn evaluates once per frame; a step that is still waiting for a
// coroutine returns false and is retried next frame
void Garage::StartSpawn() {
    std::vector<Shared::Sequence::Step> steps;

    steps.push_back([]() -> bool {
        Il2CppClass* parentClass = Il2Cpp::FindClass("CarParent");
        Il2CppClass* godClass = Il2Cpp::FindClass("GodConstant");
        Il2CppClass* carLocalClass = Il2Cpp::FindClass("CarLocalCustom");
        Il2CppClass* chassisEnum = Il2Cpp::FindClass("car_carOrigin.ChassisType");
        Il2CppClass* setupEnum = Il2Cpp::FindClass("CarParent.CarSetupType");

        if (!parentClass || !godClass || !carLocalClass || !chassisEnum || !setupEnum) {
            spawn.Fail("Required game classes were not found");
            return true;
        }

        // GodConstant owns the live CarParent reference, so take the parent
        // from there instead of scanning for a separate instance of it
        Il2CppObject* god = Il2Cpp::FindObjectOfType(godClass);
        if (!god) god = Il2Cpp::GetStaticFieldObject(godClass, "_instance");
        if (!god) { spawn.Fail("GodConstant not found"); return true; }

        Il2CppObject* parent = Il2Cpp::GetInstanceFieldObject(god, "carParent");
        if (!parent) { spawn.Fail("GodConstant.carParent is not set"); return true; }

        std::vector<Il2Cpp::EnumMember> setupMembers = Il2Cpp::GetEnumMembers(setupEnum);
        const Il2Cpp::EnumMember* setup = Il2Cpp::FindEnumMember(setupMembers, "full_Combine");
        if (!setup) { spawn.Fail("CarSetupType.full_Combine not found"); return true; }

        const MethodInfo* spawnShopCar = Il2Cpp::GetMethod(parentClass, "spawnShopCar", 5);
        const MethodInfo* shopCarSpawn = Il2Cpp::GetMethod(parentClass, "ShopCarSpawn", 5);
        const MethodInfo* saveCar = Il2Cpp::GetMethod(godClass, "saveCar_firstTime", 2);
        if (!spawnShopCar || !shopCarSpawn || !saveCar) {
            spawn.Fail("Required game methods were not found");
            return true;
        }

        // var cars = new ChassisType[] { chassis };
        Il2CppArray* cars = Il2Cpp::NewArray(chassisEnum, 1);
        if (!cars) { spawn.Fail("Failed to allocate the chassis array"); return true; }
        Il2Cpp::ArraySetRaw(cars, 0, ctx.chassisValue.data(), ctx.chassisValue.size());

        // var models = null; the game's own shop passes null here and the
        // coroutine treats it as "no model filter", accepting a candidate on
        // the rarity roll alone
        Il2CppObject* nullObject = nullptr;

        // var r1 = parent.spawnShopCar(cars, null, 0, 0, true);
        Il2Cpp::SetInstanceFieldObject(parent, "spawnShopCar_result", nullObject);
        int32_t targetCounty = 0;
        int32_t minRarity = 0;
        bool onlyStock = ctx.overrides.applyToggle ? ctx.overrides.onlyStock : true;
        void* spawnShopCarArgs[5] = {cars, nullObject, &targetCounty, &minRarity, &onlyStock};
        Il2CppObject* enumerator = Il2Cpp::Invoke(spawnShopCar, parent, spawnShopCarArgs);
        if (!enumerator) { spawn.Fail("spawnShopCar returned null"); return true; }

        Il2Cpp::StartCoroutine(parent, enumerator);

        ctx.parent = parent;
        ctx.god = god;
        ctx.carLocalClass = carLocalClass;
        ctx.shopCarSpawnMethod = shopCarSpawn;
        ctx.saveCarMethod = saveCar;
        ctx.setupValue = setup->raw;
        return true;
    });

    steps.push_back([]() -> bool {
        Il2CppObject* targetCar = Il2Cpp::GetInstanceFieldObject(ctx.parent, "spawnShopCar_result");
        if (!targetCar) return false;

        // An unsupported or unloaded chassis can yield a car_overwrite whose
        // carOrigin is null, which makes ShopCarSpawn dereference null. Reject
        // it here so the failure is reported instead of thrown inside the coroutine
        if (!Il2Cpp::GetInstanceFieldObject(targetCar, "carOrigin")) {
            spawn.Fail("Spawned car has no origin (unsupported chassis)");
            return true;
        }

        ApplyOverrides(targetCar, ctx.overrides);

        // var r2 = parent.ShopCarSpawn(targetCar, full_Combine, spawnPoint, true, false);
        Il2Cpp::SetInstanceFieldObject(ctx.parent, "ShopCarSpawn_result", nullptr);

        // Prefer the dedicated CarSpawnPoint; fall back to the CarParent transform
        Il2CppObject* spawnPoint = nullptr;
        Il2CppObject* carSpawnPoint = Il2Cpp::GetInstanceFieldObject(ctx.parent, "CarSpawnPoint");
        if (carSpawnPoint) spawnPoint = Il2Cpp::GetGameObjectTransform(carSpawnPoint);
        if (!spawnPoint) spawnPoint = Il2Cpp::GetTransform(ctx.parent);
        if (!spawnPoint) { spawn.Fail("Failed to read the car spawn point"); return true; }

        void* shopCarSpawnArgs[5] = {
            targetCar,
            ctx.setupValue.data(),
            spawnPoint,
            &ctx.isStillCar,
            &ctx.engineRunning,
        };
        Il2CppObject* enumerator = Il2Cpp::Invoke(ctx.shopCarSpawnMethod, ctx.parent, shopCarSpawnArgs);
        if (!enumerator) { spawn.Fail("ShopCarSpawn returned null"); return true; }

        Il2Cpp::StartCoroutine(ctx.parent, enumerator);

        ctx.targetCar = targetCar;
        return true;
    });

    steps.push_back([]() -> bool {
        Il2CppObject* spawnedObject = Il2Cpp::GetInstanceFieldObject(ctx.parent, "ShopCarSpawn_result");
        if (!spawnedObject) return false;

        ctx.spawnedObject = spawnedObject;
        return true;
    });

    steps.push_back([]() -> bool {
        Il2CppObject* carLocal = Il2Cpp::GetComponent(ctx.spawnedObject, ctx.carLocalClass);
        if (!carLocal) return false;

        // var r3 = god.saveCar_firstTime(carLocal, false);
        void* saveCarArgs[2] = {carLocal, &ctx.saveToHouse};
        Il2CppObject* enumerator = Il2Cpp::Invoke(ctx.saveCarMethod, ctx.god, saveCarArgs);
        if (!enumerator) { spawn.Fail("saveCar_firstTime returned null"); return true; }

        Il2Cpp::StartCoroutine(ctx.god, enumerator);
        return true;
    });

    spawn.Start(std::move(steps),
        [](bool ok, const std::string& message) {
            busy.store(false, std::memory_order_release);
            status.Set(ok ? ("Added " + ctx.chassisName + " to the garage") : message);
        },
        std::chrono::seconds(15));
}

void Garage::AddSelectedToGarage() {
    if (!chassisLoaded.load()) return;
    if (busy.exchange(true)) return;

    // Copy what the spawn needs while we are still on the render thread, then
    // hand the state over; PumpSpawn advances it on the script thread
    SpawnContext run;
    {
        std::lock_guard<std::mutex> lock(chassisMutex);
        if (selectedChassis < 0 || selectedChassis >= static_cast<int>(chassisOptions.size())) {
            busy.store(false, std::memory_order_release);
            return;
        }
        run.chassisName = chassisOptions[selectedChassis].name;
        run.chassisValue = chassisOptions[selectedChassis].raw;
    }
    run.overrides = spawnOverrides;

    MainThread::Post([run]() {
        ctx = run;
        status.Set("Spawning " + run.chassisName + "...");
        StartSpawn();
    });
}

// Building the option list touches the runtime, so it is handed to the script
// thread. The render thread only reads the finished list once chassisLoaded is set
void Garage::LoadChassisOptionsNow() {
    chassisLoadAttempted.store(true);

    Il2CppClass* chassisEnum = Il2Cpp::FindClass("car_carOrigin.ChassisType");
    if (!chassisEnum) {
        status.Set("ChassisType enum not found");
        return;
    }

    // The first two members are null_type and generic, not real chassis
    std::vector<Il2Cpp::EnumMember> members = Il2Cpp::GetEnumMembers(chassisEnum);

    std::vector<ChassisOption> options;
    if (members.size() > 2) options.reserve(members.size() - 2);
    for (size_t i = 2; i < members.size(); ++i) {
        options.push_back({members[i].name, std::move(members[i].raw)});
    }

    const size_t optionCount = options.size();

    // Swap the finished list in under the lock so the render thread never sees
    // a half-rebuilt vector
    {
        std::lock_guard<std::mutex> lock(chassisMutex);
        chassisOptions = std::move(options);
    }

    chassisLoaded.store(optionCount != 0);
    if (optionCount != 0) {
        status.Set("Loaded " + std::to_string(optionCount) + " chassis types");
    } else {
        status.Set("No chassis values found");
    }
}

void Garage::LoadChassisOptions(bool force) {
    if (!force && (chassisLoaded.load() || chassisLoadAttempted.load())) return;
    if (chassisLoadPending.exchange(true)) return;

    MainThread::Post([]() {
        LoadChassisOptionsNow();
        chassisLoadPending.store(false, std::memory_order_release);
    });
}

void Garage::RenderOverrides() {
    ImGui::Checkbox("Apply overrides", &spawnOverrides.applyToggle);

    if (!spawnOverrides.applyToggle) return;

    ImGui::Indent();
    ImGui::Checkbox("Engine", &spawnOverrides.engineToggle);
    if (spawnOverrides.engineToggle) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputFloat("##engine", &spawnOverrides.engine, 1.0f, 10.0f, "%.1f");
    }

    ImGui::Checkbox("Fuel tank", &spawnOverrides.fuelToggle);
    if (spawnOverrides.fuelToggle) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputFloat("##fuel", &spawnOverrides.fuel, 1.0f, 10.0f, "%.1f");
    }

    ImGui::PushID("mileageToggle");
    ImGui::Checkbox("Mileage", &spawnOverrides.mileageToggle);
    ImGui::PopID();
    if (spawnOverrides.mileageToggle) {
        ImGui::Indent();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputFloat("Mileage", &spawnOverrides.mileage, 1.0f, 100.0f, "%.1f");
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputFloat("Dirt mileage", &spawnOverrides.dirtMileage, 1.0f, 100.0f, "%.1f");
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputFloat("Mileage since oil change", &spawnOverrides.mileageSinceLastOilChange, 1.0f, 100.0f, "%.1f");
        ImGui::Unindent();
    }

    ImGui::Checkbox("Gearbox", &spawnOverrides.gearboxToggle);
    if (spawnOverrides.gearboxToggle) {
        ImGui::Indent();
        if (ImGui::RadioButton("Manual", !spawnOverrides.gearbox)) {
            spawnOverrides.gearbox = false;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Automatic", spawnOverrides.gearbox)) {
            spawnOverrides.gearbox = true;
        }
        ImGui::Unindent();
    }

    ImGui::Checkbox("Paint", &spawnOverrides.paintToggle);
    if (spawnOverrides.paintToggle) {
        ImGui::SameLine();
        ImGui::ColorEdit3("##paint", spawnOverrides.paint, ImGuiColorEditFlags_NoInputs);
    }

    ImGui::Checkbox("Only stock", &spawnOverrides.onlyStock);
    ImGui::Unindent();
}

void Garage::PumpSpawn() {
    spawn.Pump();
}

void Garage::RenderTab() {
    if (!Shared::BeginGameTab("Garage")) return;

    status.Render();

    LoadChassisOptions();

    if (!chassisLoaded.load()) {
        ImGui::TextUnformatted("No chassis types available.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Refresh")) {
            LoadChassisOptions(true);
        }
    } else {
        // Copy the list so the script thread can replace it without racing the widgets
        std::vector<ChassisOption> options;
        {
            std::lock_guard<std::mutex> lock(chassisMutex);
            options = chassisOptions;
        }

        if (options.empty()) {
            ImGui::TextUnformatted("No chassis types available.");
            ImGui::SameLine();
            if (ImGui::SmallButton("Refresh")) {
                LoadChassisOptions(true);
            }
            ImGui::EndTabItem();
            return;
        }

        if (selectedChassis < 0 || selectedChassis >= static_cast<int>(options.size())) {
            selectedChassis = 0;
        }

        const char* preview = options[selectedChassis].name.c_str();
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::BeginCombo("##carCombo", preview)) {
            for (int i = 0; i < static_cast<int>(options.size()); ++i) {
                const bool selected = (i == selectedChassis);
                if (ImGui::Selectable(options[i].name.c_str(), selected)) {
                    selectedChassis = i;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();

        ImGui::BeginDisabled(busy.load());
        if (ImGui::Button("Add to Garage")) {
            AddSelectedToGarage();
        }
        ImGui::EndDisabled();

        RenderOverrides();
    }

    ImGui::EndTabItem();
}
