#pragma once

#include "framework.h"

class Injector {
public:
    Injector() = delete;

    // Returns the PID of the first process whose image name matches `name` (case-insensitive, `name` should include ".exe"), or 0.
    static DWORD FindProcessId(const std::wstring& name);

    // Returns true when a module whose file name matches `module` is loaded in the process (case-insensitive).
    static bool IsModuleLoaded(DWORD processId, const std::wstring& module);

    // Classic LoadLibraryW remote-thread injection.
    static bool Inject(DWORD processId, const std::wstring& dllPath, std::wstring& error);
};
