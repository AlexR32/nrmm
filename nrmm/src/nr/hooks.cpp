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
#include "sky.h"
#include "shared.h"
#include "backends/d3d11.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"
#include "core/console.h"

// Hook state

// MinHook plumbing shared by every detour

bool Hooks::EnableDetour(void* target, void* detour, void** original, const char* name) {
    if (MH_CreateHook(target, detour, original) != MH_OK) {
        Console::Logf("[Hooks] Failed to create the {} hook", name);
        return false;
    }

    if (MH_EnableHook(target) != MH_OK) {
        Console::Logf("[Hooks] Failed to enable the {} hook", name);
        MH_RemoveHook(target);
        *original = nullptr;
        return false;
    }

    Console::Logf("[Hooks] {} hooked", name);
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
    Sky::Pump();

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

// GodConstant.doesCarPassRestriction: the crew restriction bypass

bool __fastcall Hooks::HookedDoesCarPassRestriction(Il2CppObject* self, int32_t targetRestrict, Il2CppObject* targetCar, const MethodInfo* method) {
    if (Player::disableCrewRestrictions.load(std::memory_order_relaxed)) return true;
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
    if (Player::disableCrewRestrictions.load(std::memory_order_relaxed)) return true;
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

// AzureProfileController.GetProfileSettings: the Sky tab's settings overrides

Il2CppObject* __fastcall Hooks::HookedGetProfileSettings(Il2CppObject* self, const MethodInfo* method) {
    HookScope scope;

    Il2CppObject* settings = originalGetProfileSettings ? originalGetProfileSettings(self, method) : nullptr;
    if (settings) Sky::ApplySettings(settings);
    return settings;
}

void Hooks::EnsureAzureHook() {
    if (azureHookInstalled.load(std::memory_order_acquire)) return;

    Il2CppClass* controllerClass = Il2Cpp::FindClass("UnityEngine.AzureSky.AzureProfileController");
    if (!controllerClass) return;

    const MethodInfo* method = Il2Cpp::GetMethod(controllerClass, "GetProfileSettings", 0);
    void* target = GetMethodPointer(method);
    if (!target) return;

    // A MinHook failure here is not transient, so stop retrying every tick
    if (!EnableDetour(target, reinterpret_cast<void*>(&HookedGetProfileSettings), reinterpret_cast<void**>(&originalGetProfileSettings), "AzureProfileController.GetProfileSettings")) {
        azureHookInstalled.store(true, std::memory_order_release);
        return;
    }

    azureHookTarget = target;
    azureHookInstalled.store(true, std::memory_order_release);
}

// UnityEngine.Debug: mirror the game's console output into the mod console

void __fastcall Hooks::HookedDebugLog(Il2CppObject* message, const MethodInfo* method) {
    Shared::LogUnity(Shared::ManagedToString(message), Console::Color::Default);
    if (originalDebugLog) originalDebugLog(message, method);
}

void __fastcall Hooks::HookedDebugLogWarning(Il2CppObject* message, const MethodInfo* method) {
    Shared::LogUnity(Shared::ManagedToString(message), Console::Color::Yellow);
    if (originalDebugLogWarning) originalDebugLogWarning(message, method);
}

void __fastcall Hooks::HookedDebugLogError(Il2CppObject* message, const MethodInfo* method) {
    Shared::LogUnity(Shared::ManagedToString(message), Console::Color::Red);
    if (originalDebugLogError) originalDebugLogError(message, method);
}

void __fastcall Hooks::HookedDebugLogException(Il2CppObject* exception, const MethodInfo* method) {
    Shared::LogUnity(Shared::ManagedToString(exception), Console::Color::Magenta);
    if (originalDebugLogException) originalDebugLogException(exception, method);
}

void Hooks::EnsureDebugLogHook() {
    if (debugLogHookInstalled.load(std::memory_order_acquire)) return;

    // Debug lives in the engine's core module, not Assembly-CSharp
    Il2CppImage* coreModule = Il2Cpp::GetImage("UnityEngine.CoreModule");
    if (!coreModule) return;

    Il2CppClass* debugClass = Il2Cpp::FindClass("UnityEngine.Debug", coreModule);
    if (!debugClass) return;

    const auto install = [debugClass](const char* methodName, const char* label, void* detour, void** original, void** target) {
        const MethodInfo* method = Il2Cpp::GetMethod(debugClass, methodName, 1);
        void* pointer = GetMethodPointer(method);
        if (!pointer) return;

        if (!EnableDetour(pointer, detour, original, label)) return;
        *target = pointer;
    };

    install("Log", "UnityEngine.Debug.Log",
        reinterpret_cast<void*>(&HookedDebugLog),
        reinterpret_cast<void**>(&originalDebugLog), &debugLogTarget);

    install("LogWarning", "UnityEngine.Debug.LogWarning",
        reinterpret_cast<void*>(&HookedDebugLogWarning),
        reinterpret_cast<void**>(&originalDebugLogWarning), &debugWarningHookTarget);

    install("LogError", "UnityEngine.Debug.LogError",
        reinterpret_cast<void*>(&HookedDebugLogError),
        reinterpret_cast<void**>(&originalDebugLogError), &debugErrorHookTarget);

    install("LogException", "UnityEngine.Debug.LogException",
        reinterpret_cast<void*>(&HookedDebugLogException),
        reinterpret_cast<void**>(&originalDebugLogException), &debugExceptionHookTarget);

    debugLogHookInstalled.store(true, std::memory_order_release);
}

// Lifecycle

void Hooks::EnsureInstalled() {
    EnsureUpdateHook();
    EnsureRestrictionHook();
    EnsureCrewSpecHook();
    EnsureAzureHook();
    EnsureDebugLogHook();
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

    // The Azure settings hook is independent of everything else
    DisableDetour(azureHookTarget);
    azureHookTarget = nullptr;
    originalGetProfileSettings = nullptr;
    azureHookInstalled.store(false, std::memory_order_release);

    // The Unity log hooks are installed independently of everything else
    DisableDetour(debugLogTarget);
    debugLogTarget = nullptr;
    originalDebugLog = nullptr;

    DisableDetour(debugWarningHookTarget);
    debugWarningHookTarget = nullptr;
    originalDebugLogWarning = nullptr;

    DisableDetour(debugErrorHookTarget);
    debugErrorHookTarget = nullptr;
    originalDebugLogError = nullptr;

    DisableDetour(debugExceptionHookTarget);
    debugExceptionHookTarget = nullptr;
    originalDebugLogException = nullptr;

    debugLogHookInstalled.store(false, std::memory_order_release);
}
