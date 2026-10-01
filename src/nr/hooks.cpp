#include "pch.h"

#include "hooks.h"
#include "auction.h"
#include "enums.h"
#include "garage.h"
#include "player.h"
#include "world.h"
#include "current_car.h"
#include "fixes.h"
#include "shared.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"
#include "core/logger.h"

Hooks::GodConstantUpdateFn Hooks::originalGodConstantUpdate = nullptr;
void* Hooks::godConstantUpdateTarget = nullptr;
bool Hooks::installed = false;
int Hooks::installAttempts = 0;

void* Hooks::GetMethodPointer(const MethodInfo* method) {
    if (!method) return nullptr;

    void* pointer = nullptr;
    memcpy(&pointer, method, sizeof(pointer));
    return pointer;
}

void __fastcall Hooks::HookedGodConstantUpdate(Il2CppObject* self, const MethodInfo* method) {
    // We are on the script/main thread here: bind it and run every menu action
    // that the render thread handed over this frame
    MainThread::Pump();

    // Resolve every game enum once, on the script thread, the first time the
    // game updates. The tabs then only read the cached lists
    Enums::LoadAll();

    Shared::inMainMenu = Shared::IsInMainMenu();
    Shared::inGarage = Shared::IsInGarage();

    // Read the state the menu displays while we are on the main thread, so the
    // render thread never touches managed objects for its widgets
    Player::RefreshSnapshot();
    World::RefreshSnapshot();
    CurrentCar::RefreshSnapshot();
    Fixes::RefreshSnapshot();
    Auction::RefreshSnapshot();

    Garage::PumpSpawn();

    if (originalGodConstantUpdate) originalGodConstantUpdate(self, method);
}

bool Hooks::Install() {
    if (installed) return true;

    Il2CppClass* godClass = Il2Cpp::FindClass("GodConstant");
    if (!godClass) return false;

    const MethodInfo* update = Il2Cpp::GetMethod(godClass, "Update", 0);
    if (!update) return false;

    void* target = GetMethodPointer(update);
    if (!target) return false;

    if (MH_CreateHook(target, reinterpret_cast<void*>(&HookedGodConstantUpdate), reinterpret_cast<void**>(&originalGodConstantUpdate)) != MH_OK) {
        Logger::Log("[Hooks] Failed to create the GodConstant.Update hook");
        return false;
    }

    if (MH_EnableHook(target) != MH_OK) {
        Logger::Log("[Hooks] Failed to enable the GodConstant.Update hook");
        MH_RemoveHook(target);
        originalGodConstantUpdate = nullptr;
        return false;
    }

    godConstantUpdateTarget = target;
    installed = true;
    Logger::Log("[Hooks] GodConstant.Update hooked, pumping on the script thread");
    return true;
}

void Hooks::EnsureInstalled() {
    if (installed) return;
    if (installAttempts++ % 60 != 0) return;
    Install();
}

void Hooks::Remove() {
    if (!installed || !godConstantUpdateTarget) return;

    MH_DisableHook(godConstantUpdateTarget);
    MH_RemoveHook(godConstantUpdateTarget);

    godConstantUpdateTarget = nullptr;
    originalGodConstantUpdate = nullptr;
    installed = false;
}
