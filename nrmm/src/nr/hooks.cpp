#include "pch.h"

#include "hooks.h"
#include "auction.h"
#include "enums.h"
#include "garage.h"
#include "player.h"
#include "world.h"
#include "current_car.h"
#include "fixes.h"
#include "music.h"
#include "loading_video.h"
#include "shared.h"
#include "backends/d3d11.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"
#include "core/logger.h"

// Hook state

Hooks::GodConstantUpdateFn Hooks::originalGodConstantUpdate = nullptr;
void* Hooks::updateHookTarget = nullptr;
std::atomic_bool Hooks::updateHookInstalled{ false };

Hooks::DoesCarPassRestrictionFn Hooks::originalDoesCarPassRestriction = nullptr;
void* Hooks::restrictionHookTarget = nullptr;
std::atomic_bool Hooks::restrictionHookInstalled{ false };

Hooks::DoesPlayerMeetCrewSpecFn Hooks::originalDoesPlayerMeetCrewSpec = nullptr;
void* Hooks::crewSpecHookTarget = nullptr;
std::atomic_bool Hooks::crewSpecHookInstalled{ false };

std::atomic_int Hooks::inFlight{ 0 };

// MinHook plumbing shared by every detour

bool Hooks::EnableDetour(void* target, void* detour, void** original, const char* name) {
    if (MH_CreateHook(target, detour, original) != MH_OK) {
        Logger::Logf("[Hooks] Failed to create the {} hook", name);
        return false;
    }

    if (MH_EnableHook(target) != MH_OK) {
        Logger::Logf("[Hooks] Failed to enable the {} hook", name);
        MH_RemoveHook(target);
        *original = nullptr;
        return false;
    }

    Logger::Logf("[Hooks] {} hooked", name);
    return true;
}

void Hooks::DisableDetour(void* target) {
    if (!target) return;
    MH_DisableHook(target);
    MH_RemoveHook(target);
}

void* Hooks::GetMethodPointer(const MethodInfo* method) {
    if (!method) return nullptr;

    void* pointer = nullptr;
    memcpy(&pointer, method, sizeof(pointer));
    return pointer;
}

// GodConstant.Update: the script-thread pump

void __fastcall Hooks::HookedGodConstantUpdate(Il2CppObject* self, const MethodInfo* method) {
    // Count in first so Remove cannot free the trampoline out from under an
    // in-flight call
    HookScope scope;

    // Once the game starts quitting, stop touching managed state entirely:
    // Unity is tearing the runtime down and invoking into it here is what makes
    // a graceful quit hang. The last thing done before that is releasing the
    // music player, which has to happen on this script thread
    if (D3D11Hook::shuttingDown.load(std::memory_order_acquire)) {
        Music::Shutdown();
        LoadingVideo::Shutdown();
        if (originalGodConstantUpdate) originalGodConstantUpdate(self, method);
        return;
    }

    if (!updateHookInstalled.load(std::memory_order_acquire)) {
        if (originalGodConstantUpdate) originalGodConstantUpdate(self, method);
        return;
    }

    // We are on the script/main thread here: bind it and run every menu action
    // that the render thread handed over this frame
    MainThread::Pump();

    // Resolve every game enum once, on the script thread, the first time the
    // game updates. The tabs then only read the cached lists
    Enums::LoadAll();

    Shared::inMainMenu = Shared::IsInMainMenu();
    // Shared::inGarage = Shared::IsInGarage();

    // Read the state the menu displays while we are on the main thread, so the
    // render thread never touches managed objects for its widgets
    Fixes::RefreshSnapshot();
    Player::RefreshSnapshot();
    CurrentCar::RefreshSnapshot();
    Auction::RefreshSnapshot();
    World::RefreshSnapshot();

    Garage::Pump();
    Music::Pump();
    LoadingVideo::Pump();

    if (originalGodConstantUpdate) originalGodConstantUpdate(self, method);
}

void Hooks::EnsureUpdateHook() {
    if (updateHookInstalled.load(std::memory_order_acquire)) return;

    Il2CppClass* godClass = Il2Cpp::FindClass("GodConstant");
    if (!godClass) return;

    const MethodInfo* method = Il2Cpp::GetMethod(godClass, "Update", 0);
    void* target = GetMethodPointer(method);
    if (!target) return;

    // A MinHook failure here is not transient, so stop retrying every tick
    if (!EnableDetour(target, reinterpret_cast<void*>(&HookedGodConstantUpdate), reinterpret_cast<void**>(&originalGodConstantUpdate), "GodConstant.Update")) {
        updateHookInstalled.store(true, std::memory_order_release);
        return;
    }

    updateHookTarget = target;
    updateHookInstalled.store(true, std::memory_order_release);
}

// GodConstant.doesCarPassRestriction: the meetspot-restriction bypass

bool __fastcall Hooks::HookedDoesCarPassRestriction(Il2CppObject* self, int32_t targetRestrict, Il2CppObject* targetCar, const MethodInfo* method) {
    // The toggle lives on the Player tab; when set the player's car passes every
    // crew restriction, so meetSpot.startProcess never marks it as failing
    if (Player::disableMeetspotRestrictions.load(std::memory_order_relaxed)) return true;
    if (originalDoesCarPassRestriction) return originalDoesCarPassRestriction(self, targetRestrict, targetCar, method);
    return false;
}

void Hooks::EnsureRestrictionHook() {
    if (restrictionHookInstalled.load(std::memory_order_acquire)) return;

    Il2CppClass* godClass = Il2Cpp::FindClass("GodConstant");
    if (!godClass) return;

    const MethodInfo* method = Il2Cpp::GetMethod(godClass, "doesCarPassRestriction", 2);
    void* target = GetMethodPointer(method);
    if (!target) return;

    // A MinHook failure here is not transient, so stop retrying every tick
    if (!EnableDetour(target, reinterpret_cast<void*>(&HookedDoesCarPassRestriction), reinterpret_cast<void**>(&originalDoesCarPassRestriction), "GodConstant.doesCarPassRestriction")) {
        restrictionHookInstalled.store(true, std::memory_order_release);
        return;
    }

    restrictionHookTarget = target;
    restrictionHookInstalled.store(true, std::memory_order_release);
}

// GodConstant.DoesPlayerMeetCrewSpec: the meetspot crew-spec bypass

bool __fastcall Hooks::HookedDoesPlayerMeetCrewSpec(Il2CppObject* self, float playerHP, float playerHandling, float playerBrake, int32_t targetCrew, float originSpecFloor, const MethodInfo* method) {
    // Same meetspot toggle: pretend the player's car always matches the crew spec
    if (Player::disableMeetspotRestrictions.load(std::memory_order_relaxed)) return true;
    if (originalDoesPlayerMeetCrewSpec) return originalDoesPlayerMeetCrewSpec(self, playerHP, playerHandling, playerBrake, targetCrew, originSpecFloor, method);
    return false;
}

void Hooks::EnsureCrewSpecHook() {
    if (crewSpecHookInstalled.load(std::memory_order_acquire)) return;

    Il2CppClass* godClass = Il2Cpp::FindClass("GodConstant");
    if (!godClass) return;

    const MethodInfo* method = Il2Cpp::GetMethod(godClass, "DoesPlayerMeetCrewSpec", 5);
    void* target = GetMethodPointer(method);
    if (!target) return;

    // A MinHook failure here is not transient, so stop retrying every tick
    if (!EnableDetour(target, reinterpret_cast<void*>(&HookedDoesPlayerMeetCrewSpec), reinterpret_cast<void**>(&originalDoesPlayerMeetCrewSpec), "GodConstant.DoesPlayerMeetCrewSpec")) {
        crewSpecHookInstalled.store(true, std::memory_order_release);
        return;
    }

    crewSpecHookTarget = target;
    crewSpecHookInstalled.store(true, std::memory_order_release);
}

// Lifecycle

void Hooks::EnsureInstalled() {
    EnsureUpdateHook();
    EnsureRestrictionHook();
    EnsureCrewSpecHook();
}

void Hooks::Remove() {
    updateHookInstalled.store(false, std::memory_order_release);

    // Tear the pump down first: disable it so no new detour starts, wait for an
    // in-flight one to fall through (bounded so a wedge during teardown cannot
    // hang), then remove it
    if (updateHookTarget) {
        MH_DisableHook(updateHookTarget);

        const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(5000);
        while (inFlight.load(std::memory_order_acquire) != 0 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        MH_RemoveHook(updateHookTarget);
        updateHookTarget = nullptr;
        originalGodConstantUpdate = nullptr;
    }

    // The meetspot hooks are installed independently of the pump
    DisableDetour(restrictionHookTarget);
    restrictionHookTarget = nullptr;
    originalDoesCarPassRestriction = nullptr;
    restrictionHookInstalled.store(false, std::memory_order_release);

    DisableDetour(crewSpecHookTarget);
    crewSpecHookTarget = nullptr;
    originalDoesPlayerMeetCrewSpec = nullptr;
    crewSpecHookInstalled.store(false, std::memory_order_release);
}
