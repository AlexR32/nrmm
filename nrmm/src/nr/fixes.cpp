#include "pch.h"

#include "fixes.h"
#include "shared.h"
#include "il2cpp/il2cpp.h"
#include "core/logger.h"

std::atomic_bool Fixes::chassisFixed{false};

// Game-bug workarounds

// The game ships the Tobimizu Wasp (model 33) on the Kymoto Sorana hatch chassis (21)
// instead of its own chassis (23), so every car_carOrigin entry with that pair is corrected
void Fixes::FixChassis() {
    if (chassisFixed.exchange(true)) return;

    constexpr int32_t kSoranaHatchChassis = 21; // car_carOrigin.ChassisType.kymoto_sorana_hatch_1983_1987
    constexpr int32_t kWaspChassis = 23;        // car_carOrigin.ChassisType.tobimizu_wasp_1996
    constexpr int32_t kWaspModel = 33;          // car_carOrigin.ModelType.tobimizu_wasp_1996

    Il2CppClass* containerClass = Il2Cpp::FindClass("container_allCarOrigins");
    if (!containerClass) {
        chassisFixed.store(false, std::memory_order_relaxed);
        return;
    }

    std::vector<Il2CppObject*> candidates;
    Il2CppArray* containers = Il2Cpp::FindObjectsOfTypeAll(containerClass);
    const size_t containerCount = containers ? Il2Cpp::ArrayLength(containers) : 0;
    for (size_t i = 0; i < containerCount; ++i) {
        if (Il2CppObject* container = Il2Cpp::ArrayGetRef(containers, i)) candidates.push_back(container);
    }

    Il2CppObject* allCars = nullptr;
    const MethodInfo* countMethod = nullptr;
    const MethodInfo* itemMethod = nullptr;
    int32_t count = 0;

    for (Il2CppObject* container : candidates) {
        if (count > 0) break;
        if (!Il2Cpp::IsUnityObjectAlive(container)) continue;

        Il2CppObject* cars = Il2Cpp::GetInstanceFieldObject(container, "all_cars");
        if (!cars) continue;

        const MethodInfo* getCount = Il2Cpp::GetMethod(Il2Cpp::ObjectClass(cars), "get_Count", 0);
        const MethodInfo* getItem = Il2Cpp::GetMethod(Il2Cpp::ObjectClass(cars), "get_Item", 1);
        if (!getCount || !getItem) continue;

        const int32_t carsCount = Il2Cpp::UnboxInt32(Il2Cpp::Invoke(getCount, cars, nullptr));
        if (carsCount <= 0) continue;

        allCars = cars;
        countMethod = getCount;
        itemMethod = getItem;
        count = carsCount;
    }

    if (count <= 0) {
        chassisFixed.store(false, std::memory_order_relaxed); // not loaded yet, retry next frame
        return;
    }

    for (int32_t i = 0; i < count; ++i) {
        void* args[1] = {&i};
        Il2CppObject* car = Il2Cpp::Invoke(itemMethod, allCars, args);
        if (!car) continue;

        int32_t chassis = 0;
        int32_t model = 0;
        if (!Il2Cpp::GetInstanceFieldValue(car, "chassisType", chassis)) continue;
        if (!Il2Cpp::GetInstanceFieldValue(car, "modelType", model)) continue;

        if (model == kWaspModel && chassis == kSoranaHatchChassis) {
            Il2Cpp::SetInstanceFieldValue(car, "chassisType", kWaspChassis);
            Logger::Log("[Fixes] Corrected TK_ZZ chassis type to tobimizu_wasp_1996");
        }
    }
}

// Snapshot / tab

void Fixes::RefreshSnapshot() {
    Il2Cpp::ThreadAttach();

    // Drivetrain fix: the game resets RCC_CarControllerV3._wheelTypeOriginal to
    // RWD whenever a car spawns. CarData.drivetrain keeps the drivetrain the car
    // is actually saved with, and the two enums share the same ordinals
    // (FWD, RWD, AWD, BIASED), so copy the saved value back onto the controller
    // every frame and the car keeps the drivetrain it is supposed to have
    Il2CppObject* car = Shared::PlayerCar();
    Il2CppObject* carData = Shared::CarData();
    int32_t drivetrain = 0;
    if (car && carData && Il2Cpp::GetInstanceFieldValue(carData, "drivetrain", drivetrain)) {
        int32_t controllerDrivetrain = 0;
        // Only write when it drifted: avoids a managed field write every frame
        if (!Il2Cpp::GetInstanceFieldValue(car, "_wheelTypeOriginal", controllerDrivetrain) || controllerDrivetrain != drivetrain) {
            Il2Cpp::SetInstanceFieldValue(car, "_wheelTypeOriginal", drivetrain);
        }
    }

    // Chassis fix: one-shot, runs once the container is loaded
    FixChassis();
}

void Fixes::RenderTab() {
    if (!Shared::BeginGameTab("Fixes")) return;

    // The sync runs every frame on the script thread, so the checkbox is a
    // status indicator rather than a toggle: it stays checked and disabled
    bool enabled = true;
    ImGui::BeginDisabled();
    ImGui::Checkbox("Drivetrain sync", &enabled);
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("In the game, all cars are forced to have rear-wheel drive, regardless of the transmission type");

    ImGui::BeginDisabled();
    ImGui::Checkbox("Chassis mismatch", &enabled);
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Some cars have wrong Chassis type probably because of typo");

    ImGui::EndTabItem();
}
