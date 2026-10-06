#pragma once
#include "pch.h"
#include "il2cpp/il2cpp.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Every game enum the menu exposes is resolved once, on the script thread, from
// the GodConstant.Update hook (see Hooks::HookedGodConstantUpdate). The tabs
// only read the cached lists, so a widget never triggers managed reflection and
// there is a single place that knows the managed class name of each enum

class Enums {
public:
    enum class Id {
        Drivetrain,    // CarData.Drivetrain
        InductionType, // CarData.InductionType
        FuelType,      // CarLocalCustom.Car_FuelType
        RaceCrews,     // GodConstant.Race_Crews
        MeetSpots,     // GodConstant.MeetSpots_All
        HouseSaveIds,  // homegarageSpot.HouseSaveID
        NumberPlateType, // car_numberPlate.NumberPlateType
        Count,
    };

    // Resolves every list on the script thread. Safe to call every frame: the
    // work only happens once. Queued through MainThread so it also runs when
    // called before the script thread is known
    static void LoadAll();

    // True once LoadAll has finished on the script thread. Every accessor below
    // must only be read after this is true
    static bool Ready();

    // The cached list. Empty until Ready()
    static const std::vector<Il2Cpp::EnumMember>& Get(Id id);

    static const std::vector<Il2Cpp::EnumMember>& DrivetrainTypes();
    static const std::vector<Il2Cpp::EnumMember>& InductionTypes();
    static const std::vector<Il2Cpp::EnumMember>& FuelTypes();
    static const std::vector<Il2Cpp::EnumMember>& RaceCrews();
    static const std::vector<Il2Cpp::EnumMember>& MeetSpots();
    static const std::vector<Il2Cpp::EnumMember>& HouseSaveIds();
    static const std::vector<Il2Cpp::EnumMember>& PlateTypes();

    // Index of the literal whose raw bytes match, or -1 when it is not listed
    static int IndexOf(Id id, const void* raw, size_t size);

    // Index of an instance's enum field value, or -1 when the field or value is
    // absent
    static int SyncIndex(Id id, Il2CppObject* instance, const char* fieldName);

    // Display name of the literal, or "<fallbackPrefix><value>" when it is not
    // listed
    static std::string NameOf(Id id, int32_t value, const char* fallbackPrefix);

private:
    static void LoadAllInline();
    static const char* ClassName(Id id);

    static std::vector<Il2Cpp::EnumMember> lists[static_cast<size_t>(Id::Count)];
    static std::atomic_bool loaded;
    static std::atomic_bool requested;
};
