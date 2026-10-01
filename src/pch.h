#pragma once

#ifndef PCH_H
#define PCH_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// STANDARD LIBRARIES
#include <windows.h>
#include <iostream>
#include <string>
#include <format>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <string_view>
#include <cassert>
#include <atomic>
#include <functional>
#include <chrono>
#include <mutex>
#include <deque>
#include <array>
#include <cstdint>
#include <cstddef>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <type_traits>
#include <utility>

// DIRECTX 11
#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

// MINHOOK
#include "../libs/minhook/include/MinHook.h"

// IMGUI
#include "../libs/imgui/imgui.h"
#include "../libs/imgui/imgui_internal.h"
#include "../libs/imgui/imgui_impl_win32.h"
#include "../libs/imgui/imgui_impl_dx11.h"

#endif
