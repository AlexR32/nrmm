#pragma once
#include "pch.h"
#include "il2cpp/types.h"
#include <atomic>

// Script-thread hooks
// Unity only advances coroutines from its player loop, so anything that starts
// them must run on the game's script thread. Three detours are installed:
//   - GodConstant.Update, the script-thread pump: it drains the MainThread
//     queue, refreshes the menu snapshots and pumps the garage/audio sequences
//   - GodConstant.doesCarPassRestriction and GodConstant.DoesPlayerMeetCrewSpec,
//     which back the Player tab's meetspot-restriction toggle

class Hooks {
public:
    static void EnsureInstalled();
    static void Remove();

private:
    using GodConstantUpdateFn = void(__fastcall*)(Il2CppObject* self, const MethodInfo* method);
    using DoesCarPassRestrictionFn = bool(__fastcall*)(Il2CppObject* self, int32_t targetRestrict, Il2CppObject* targetCar, const MethodInfo* method);
    using DoesPlayerMeetCrewSpecFn = bool(__fastcall*)(Il2CppObject* self, float playerHP, float playerHandling, float playerBrake, int32_t targetCrew, float originSpecFloor, const MethodInfo* method);

    // MinHook plumbing shared by every detour. EnableDetour creates and enables
    // a detour on a resolved native target, publishing the trampoline through
    // *original; on failure the target is left untouched
    static bool EnableDetour(void* target, void* detour, void** original, const char* name);
    static void DisableDetour(void* target);

    // MethodInfo::methodPointer is the first member of the struct
    static void* GetMethodPointer(const MethodInfo* method);

    // GodConstant.Update: the script-thread pump. EnsureUpdateHook is retried
    // from EnsureInstalled until the game class and method resolve

    static void EnsureUpdateHook();
    static void __fastcall HookedGodConstantUpdate(Il2CppObject* self, const MethodInfo* method);

    static GodConstantUpdateFn originalGodConstantUpdate;
    static void* updateHookTarget;
    static std::atomic_bool updateHookInstalled;

    // GodConstant.doesCarPassRestriction: the meetspot-restriction bypass.
    // EnsureRestrictionHook is retried from EnsureInstalled until the game class
    // and method resolve

    static void EnsureRestrictionHook();
    static bool __fastcall HookedDoesCarPassRestriction(Il2CppObject* self, int32_t targetRestrict, Il2CppObject* targetCar, const MethodInfo* method);

    static DoesCarPassRestrictionFn originalDoesCarPassRestriction;
    static void* restrictionHookTarget;
    static std::atomic_bool restrictionHookInstalled;

    // GodConstant.DoesPlayerMeetCrewSpec: the meetspot crew-spec bypass, sharing
    // the same toggle. EnsureCrewSpecHook is retried from EnsureInstalled until
    // the game class and method resolve

    static void EnsureCrewSpecHook();
    static bool __fastcall HookedDoesPlayerMeetCrewSpec(Il2CppObject* self, float playerHP, float playerHandling, float playerBrake, int32_t targetCrew, float originSpecFloor, const MethodInfo* method);

    static DoesPlayerMeetCrewSpecFn originalDoesPlayerMeetCrewSpec;
    static void* crewSpecHookTarget;
    static std::atomic_bool crewSpecHookInstalled;

    // Keeps Remove() from tearing the pump detour down while it is still
    // executing on the script thread
    static std::atomic_int inFlight;

    struct HookScope {
        HookScope() { inFlight.fetch_add(1, std::memory_order_acq_rel); }
        ~HookScope() { inFlight.fetch_sub(1, std::memory_order_acq_rel); }
    };
};
