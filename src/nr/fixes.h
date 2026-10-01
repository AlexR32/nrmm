#pragma once
#include "pch.h"
#include <atomic>

class Fixes {
public:
    static void RenderTab();
    static void RefreshSnapshot();
private:
    static void FixChassis();
    static std::atomic_bool chassisFixed;
};
