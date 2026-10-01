#pragma once

#include <windows.h>
#include <atomic>

extern HMODULE g_hModule;
extern std::atomic_bool g_Running;
extern std::string g_DllPath;
