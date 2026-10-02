#pragma once
#include "pch.h"
#include "il2cpp/types.h"
#include <atomic>

// Script-thread hooks
// Unity only advances coroutines from its player loop, so anything that starts
// them must run on the game's script thread. GodConstant.Update is called there
// every frame, which makes it the one game hook the mod uses: it drains the
// MainThread queue, refreshes the player snapshot and pumps the garage sequence

class Hooks {
public:
    // Called from the mod's own worker loop, never from the render thread:
    // installing here avoids doing il2cpp reflection and MinHook work inside
    // the Present detour. GodConstant may not exist during the first frames, so
    // it is retried until it does
    static void EnsureInstalled();

    static void Remove();

private:
    using GodConstantUpdateFn = void(__fastcall*)(Il2CppObject* self, const MethodInfo* method);

    static bool Install();

    // MethodInfo::methodPointer is the first member of the struct
    static void* GetMethodPointer(const MethodInfo* method);

    static void __fastcall HookedGodConstantUpdate(Il2CppObject* self, const MethodInfo* method);

    // Keeps Remove() from tearing the detour down while it is still executing
    // on the script thread
    struct HookScope {
        HookScope() { inFlight.fetch_add(1, std::memory_order_acq_rel); }
        ~HookScope() { inFlight.fetch_sub(1, std::memory_order_acq_rel); }
    };

    static GodConstantUpdateFn originalGodConstantUpdate;
    static void* godConstantUpdateTarget;
    static std::atomic_bool installed;
    static std::atomic_bool active;
    static std::atomic_int inFlight;
};
