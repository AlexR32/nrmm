#include "pch.h"

#include "enums.h"
#include "shared.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"
#include "core/logger.h"

std::vector<Il2Cpp::EnumMember> Enums::lists[static_cast<size_t>(Enums::Id::Count)];
std::atomic_bool Enums::loaded{false};
std::atomic_bool Enums::requested{false};

const char* Enums::ClassName(Id id) {
    switch (id) {
    case Id::Drivetrain:    return "CarData.Drivetrain";
    case Id::InductionType: return "CarData.InductionType";
    case Id::FuelType:      return "CarLocalCustom.Car_FuelType";
    case Id::RaceCrews:     return "GodConstant.Race_Crews";
    case Id::MeetSpots:     return "GodConstant.MeetSpots_All";
    case Id::HouseSaveIds:  return "homegarageSpot.HouseSaveID";
    default:                return "";
    }
}

void Enums::LoadAllInline() {
    for (size_t i = 0; i < static_cast<size_t>(Id::Count); ++i) {
        const Id id = static_cast<Id>(i);
        const char* className = ClassName(id);

        Il2CppClass* klass = Il2Cpp::FindClass(className);
        if (!klass) {
            Logger::Log(std::string("[Enums] Enum not found: ") + className);
            continue;
        }

        lists[i] = Il2Cpp::GetEnumMembers(klass);
        if (lists[i].empty()) {
            Logger::Log(std::string("[Enums] No values for enum: ") + className);
        } else {
            Logger::Log("[Enums] Loaded " + std::to_string(lists[i].size()) + " values for " + className);
        }
    }
}

void Enums::LoadAll() {
    if (loaded.load(std::memory_order_acquire)) return;
    if (requested.exchange(true)) return;

    MainThread::Post([]() {
        LoadAllInline();
        loaded.store(true, std::memory_order_release);
    });
}

bool Enums::Ready() {
    return loaded.load(std::memory_order_acquire);
}

const std::vector<Il2Cpp::EnumMember>& Enums::Get(Id id) {
    const size_t index = static_cast<size_t>(id);
    if (index >= static_cast<size_t>(Id::Count)) {
        static const std::vector<Il2Cpp::EnumMember> empty;
        return empty;
    }
    return lists[index];
}

const std::vector<Il2Cpp::EnumMember>& Enums::DrivetrainTypes() { return Get(Id::Drivetrain); }
const std::vector<Il2Cpp::EnumMember>& Enums::InductionTypes() { return Get(Id::InductionType); }
const std::vector<Il2Cpp::EnumMember>& Enums::FuelTypes() { return Get(Id::FuelType); }
const std::vector<Il2Cpp::EnumMember>& Enums::RaceCrews() { return Get(Id::RaceCrews); }
const std::vector<Il2Cpp::EnumMember>& Enums::MeetSpots() { return Get(Id::MeetSpots); }
const std::vector<Il2Cpp::EnumMember>& Enums::HouseSaveIds() { return Get(Id::HouseSaveIds); }

int Enums::IndexOf(Id id, const void* raw, size_t size) {
    return Shared::FindEnumIndex(Get(id), raw, size);
}

int Enums::SyncIndex(Id id, Il2CppObject* instance, const char* fieldName) {
    int index = -1;
    Shared::SyncEnumIndex(instance, fieldName, Get(id), index);
    return index;
}

std::string Enums::NameOf(Id id, int32_t value, const char* fallbackPrefix) {
    const int index = IndexOf(id, &value, sizeof(value));
    if (index >= 0) return Get(id)[index].name;
    return std::string(fallbackPrefix ? fallbackPrefix : "") + std::to_string(value);
}
