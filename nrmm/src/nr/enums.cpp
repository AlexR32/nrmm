#include "pch.h"

#include "enums.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"
#include "core/console.h"

const char* Enums::ClassName(Id id) {
    switch (id) {
    case Id::Drivetrain:      return "CarData.Drivetrain";
    case Id::InductionType:   return "CarData.InductionType";
    case Id::FuelType:        return "CarLocalCustom.Car_FuelType";
    case Id::RaceCrews:       return "GodConstant.Race_Crews";
    case Id::MeetSpots:       return "GodConstant.MeetSpots_All";
    case Id::HouseSaveIds:    return "homegarageSpot.HouseSaveID";
    case Id::NumberPlateType: return "car_numberPlate.NumberPlateType";
    default:                  return "";
    }
}

void Enums::LoadAllInline() {
    for (size_t i = 0; i < static_cast<size_t>(Id::Count); ++i) {
        const Id id = static_cast<Id>(i);
        const char* className = ClassName(id);

        Il2CppClass* klass = Il2Cpp::FindClass(className);
        if (!klass) {
            Console::Log(std::string("[Enums] Enum not found: ") + className);
            continue;
        }

        lists[i] = Il2Cpp::GetEnumMembers(klass);
        if (lists[i].empty()) {
            Console::Log(std::string("[Enums] No values for enum: ") + className);
        } else {
            Console::Log("[Enums] Loaded " + std::to_string(lists[i].size()) + " values for " + className);
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
const std::vector<Il2Cpp::EnumMember>& Enums::PlateTypes() { return Get(Id::NumberPlateType); }

int Enums::IndexOf(Id id, const void* raw, size_t size) {
    return FindEnumIndex(Get(id), raw, size);
}

int Enums::SyncIndex(Id id, Il2CppObject* instance, const char* fieldName) {
    int index = -1;
    SyncEnumIndex(instance, fieldName, Get(id), index);
    return index;
}

std::string Enums::NameOf(Id id, int32_t value, const char* fallbackPrefix) {
    const int index = IndexOf(id, &value, sizeof(value));
    if (index >= 0) return Get(id)[index].name;
    return std::string(fallbackPrefix ? fallbackPrefix : "") + std::to_string(value);
}

void Enums::RenderEnumCombo(const char* label, const std::vector<Il2Cpp::EnumMember>& options, int selected, std::function<void(int)> onSelect) {
    const char* preview = (selected >= 0 && selected < static_cast<int>(options.size())) ? options[selected].name.c_str() : "(unknown)";
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::BeginCombo(label, preview)) {
        for (int i = 0; i < static_cast<int>(options.size()); ++i) {
            const bool isSelected = (i == selected);
            if (ImGui::Selectable(options[i].name.c_str(), isSelected)) {
                onSelect(i);
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

int32_t Enums::EnumValue(const Il2Cpp::EnumMember& member) {
    int32_t value = 0;
    if (member.raw.size() >= sizeof(int32_t)) {
        memcpy(&value, member.raw.data(), sizeof(int32_t));
    }
    return value;
}

bool Enums::SetEnumField(Il2CppObject* instance, const char* fieldName, const Il2Cpp::EnumMember& member) {
    if (!instance) return false;
    return Il2Cpp::SetInstanceFieldObject(instance, fieldName, const_cast<uint8_t*>(member.raw.data()));
}

int Enums::FindEnumIndex(const std::vector<Il2Cpp::EnumMember>& options, const void* raw, size_t size) {
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i].raw.size() == size && memcmp(options[i].raw.data(), raw, size) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void Enums::SyncEnumIndex(Il2CppObject* instance, const char* fieldName, const std::vector<Il2Cpp::EnumMember>& options, int& index) {
    index = -1;
    if (!instance) return;

    int32_t value = 0;
    Il2Cpp::GetInstanceFieldValue(instance, fieldName, value);
    index = FindEnumIndex(options, &value, sizeof(value));
}
