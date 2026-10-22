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
//   - UnityEngine.Debug.Log/LogWarning/LogError/LogException, which mirror the
//     game's own log output into the mod console

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

    inline static GodConstantUpdateFn originalGodConstantUpdate = nullptr;
    inline static void* updateHookTarget = nullptr;
    inline static std::atomic_bool updateHookInstalled{false};

    // GodConstant.doesCarPassRestriction: the meetspot-restriction bypass.
    // EnsureRestrictionHook is retried from EnsureInstalled until the game class
    // and method resolve

    static void EnsureRestrictionHook();
    static bool __fastcall HookedDoesCarPassRestriction(Il2CppObject* self, int32_t targetRestrict, Il2CppObject* targetCar, const MethodInfo* method);

    inline static DoesCarPassRestrictionFn originalDoesCarPassRestriction = nullptr;
    inline static void* restrictionHookTarget = nullptr;
    inline static std::atomic_bool restrictionHookInstalled{false};

    // GodConstant.DoesPlayerMeetCrewSpec: the meetspot crew-spec bypass, sharing
    // the same toggle. EnsureCrewSpecHook is retried from EnsureInstalled until
    // the game class and method resolve

    static void EnsureCrewSpecHook();
    static bool __fastcall HookedDoesPlayerMeetCrewSpec(Il2CppObject* self, float playerHP, float playerHandling, float playerBrake, int32_t targetCrew, float originSpecFloor, const MethodInfo* method);

    inline static DoesPlayerMeetCrewSpecFn originalDoesPlayerMeetCrewSpec = nullptr;
    inline static void* crewSpecHookTarget = nullptr;
    inline static std::atomic_bool crewSpecHookInstalled{false};

    // AzureProfileController.GetProfileSettings: the Sky tab rewrites the
    // freshly computed Azure settings here, before the game applies them to the
    // sky/fog shaders. EnsureAzureHook is retried from EnsureInstalled until the
    // class and method resolve

    using GetProfileSettingsFn = Il2CppObject* (__fastcall*)(Il2CppObject* self, const MethodInfo* method);

    static void EnsureAzureHook();
    static Il2CppObject* __fastcall HookedGetProfileSettings(Il2CppObject* self, const MethodInfo* method);

    inline static GetProfileSettingsFn originalGetProfileSettings = nullptr;
    inline static void* azureHookTarget = nullptr;
    inline static std::atomic_bool azureHookInstalled{false};

    // UnityEngine.Debug: mirrors the game's log output into the mod console.
    // Only the single-argument overloads are hooked, which is what the game
    // overwhelmingly calls. EnsureDebugLogHook is retried from EnsureInstalled
    // until the Debug class and its methods resolve

    using DebugLogFn = void(__fastcall*)(Il2CppObject* message, const MethodInfo* method);
    using DebugLogExceptionFn = void(__fastcall*)(Il2CppObject* exception, const MethodInfo* method);

    static void EnsureDebugLogHook();

    static void __fastcall HookedDebugLog(Il2CppObject* message, const MethodInfo* method);
    static void __fastcall HookedDebugLogWarning(Il2CppObject* message, const MethodInfo* method);
    static void __fastcall HookedDebugLogError(Il2CppObject* message, const MethodInfo* method);
    static void __fastcall HookedDebugLogException(Il2CppObject* exception, const MethodInfo* method);

    inline static DebugLogFn originalDebugLog = nullptr;
    inline static DebugLogFn originalDebugLogWarning = nullptr;
    inline static DebugLogFn originalDebugLogError = nullptr;
    inline static DebugLogExceptionFn originalDebugLogException = nullptr;
    inline static void* debugLogTarget = nullptr;
    inline static void* debugWarningHookTarget = nullptr;
    inline static void* debugErrorHookTarget = nullptr;
    inline static void* debugExceptionHookTarget = nullptr;
    inline static std::atomic_bool debugLogHookInstalled{false};

    // Keeps Remove() from tearing the pump detour down while it is still
    // executing on the script thread
    inline static std::atomic_int inFlight{0};

    struct HookScope {
        HookScope() { inFlight.fetch_add(1, std::memory_order_acq_rel); }
        ~HookScope() { inFlight.fetch_sub(1, std::memory_order_acq_rel); }
    };
};
