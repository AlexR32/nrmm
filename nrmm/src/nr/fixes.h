#pragma once
#include "pch.h"
#include <atomic>

class Fixes {
public:
    static void RenderTab();
    static void RefreshSnapshot();
private:
    static void FixChassis();
    inline static std::atomic_bool chassisFixed{false};
};
