#pragma once
#include "pch.h"
#include "il2cpp/types.h"

// Script-thread hooks
// Unity only advances coroutines from its player loop, so anything that starts
// them must run on the game's script thread. GodConstant.Update is called there
// every frame, which makes it the one game hook the mod uses: it drains the
// MainThread queue, refreshes the player snapshot and pumps the garage sequence

class Hooks {
public:
    // Called from the render callback. GodConstant may not exist during the
    // first frames, so it retries roughly once a second until it does
    static void EnsureInstalled();

    static void Remove();

private:
    using GodConstantUpdateFn = void(__fastcall*)(Il2CppObject* self, const MethodInfo* method);

    static bool Install();

    // MethodInfo::methodPointer is the first member of the struct
    static void* GetMethodPointer(const MethodInfo* method);

    static void __fastcall HookedGodConstantUpdate(Il2CppObject* self, const MethodInfo* method);

    static GodConstantUpdateFn originalGodConstantUpdate;
    static void* godConstantUpdateTarget;
    static bool installed;
    static int installAttempts;
};
