#pragma once

#include <windows.h>
#include <atomic>
#include <string>

extern HMODULE g_hModule;
extern std::atomic_bool g_Running;
extern std::wstring g_DllPath;
