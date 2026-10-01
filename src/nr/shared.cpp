#include "pch.h"

#include "shared.h"
#include "il2cpp/il2cpp.h"
#include "core/logger.h"

void Shared::Status::Set(const std::string& message) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        text = message;
    }
    Logger::Log(std::string("[") + tag + "] " + message);
}

std::string Shared::Status::Get() const {
    std::lock_guard<std::mutex> lock(mutex);
    return text;
}

void Shared::Status::Render() const {
    const std::string current = Get();
    if (current.empty()) return;

    ImGui::TextWrapped("%s", current.c_str());
    ImGui::Separator();
}

void Shared::Sequence::Start(std::vector<Step> newSteps, Completion done, std::chrono::milliseconds timeout) {
    steps = std::move(newSteps);
    index = 0;
    onDone = std::move(done);
    deadline = std::chrono::steady_clock::now() + timeout;
    failure.clear();
    active = true;
}

void Shared::Sequence::Fail(const std::string& message) {
    failure = message;
}

void Shared::Sequence::Finish(bool ok, const std::string& message) {
    active = false;
    steps.clear();
    index = 0;

    Completion done = std::move(onDone);
    onDone = nullptr;
    if (done) done(ok, message);
}

void Shared::Sequence::Reset() {
    active = false;
    steps.clear();
    index = 0;
    onDone = nullptr;
    failure.clear();
}

void Shared::Sequence::Pump() {
    if (!active) return;

    Il2Cpp::ThreadAttach();

    while (active) {
        if (std::chrono::steady_clock::now() >= deadline) {
            Finish(false, "Timed out");
            return;
        }

        if (index >= steps.size()) {
            Finish(true, std::string());
            return;
        }

        failure.clear();
        const bool stepDone = steps[index]();
        if (!failure.empty()) {
            Finish(false, failure);
            return;
        }
        if (!stepDone) return; // wait for the next frame

        ++index;
    }
}

void Shared::RenderLabel(const char* text) {
    const float textWidth = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textWidth) * 0.5f);
    ImGui::TextUnformatted(text);
}

bool Shared::BeginGameTab(const char* label) {
    if (!ImGui::BeginTabItem(label)) return false;

    if (GameReady()) return true;

    const char* text = inMainMenu ? "GAME NOT LOADED" : "WAITING FOR GAME THREAD";
    Shared::RenderLabel(text);
    ImGui::EndTabItem();
    return false;
}

void Shared::RenderEnumCombo(const char* label, const std::vector<Il2Cpp::EnumMember>& options, int selected, std::function<void(int)> onSelect) {
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

Il2CppObject* Shared::WaitForResultField(Il2CppObject* instance, const char* fieldName) {
    Il2CppObject* value = nullptr;
    WaitUntil([&]() {
        value = Il2Cpp::GetInstanceFieldObject(instance, fieldName);
        return value != nullptr;
    });
    return value;
}

void Shared::DestroyObject(Il2CppObject* object) {
    if (!object) return;

    Il2CppClass* objectClass = Il2Cpp::FindClass("UnityEngine.Object", Il2Cpp::GetImage("UnityEngine.CoreModule"));
    if (!objectClass) {
        Logger::Log("[Shared] UnityEngine.Object not found");
        return;
    }

    const MethodInfo* destroy = Il2Cpp::GetMethod(objectClass, "Destroy", 1);
    if (!destroy) {
        Logger::Log("[Shared] Object.Destroy not found");
        return;
    }

    void* args[1] = {object};
    Il2Cpp::Invoke(destroy, nullptr, args);
}

bool Shared::GetMainMenuMode(MainMenuMode& outMode) {
    Il2Cpp::ThreadAttach();

    Il2CppClass* menuClass = Il2Cpp::FindClass("mainMenu");
    if (!menuClass) return false;

    Il2CppObject* menu = Il2Cpp::FindObjectOfType(menuClass);
    if (!menu) return false;

    int32_t value = 0;
    if (!Il2Cpp::GetInstanceFieldValue(menu, "mainMenu_mode", value)) return false;

    outMode = static_cast<MainMenuMode>(value);
    return true;
}

bool Shared::IsInMainMenu() {
    MainMenuMode mode;
    if (!GetMainMenuMode(mode)) return false;

    switch (mode) {
    case MainMenuMode::MainMenu:
    case MainMenuMode::Null:
    case MainMenuMode::EnterName:
    case MainMenuMode::Intro:
    case MainMenuMode::Leaderboard:
        return true;
    default:
        return false;
    }
}

bool Shared::IsInGarage() {
    Il2CppObject* god = God();
    Il2CppObject* scene = god ? Il2Cpp::GetInstanceFieldObject(god, "floatingPoint_drivingScene") : nullptr;
    return scene != nullptr;
}

Il2CppObject* Shared::God() {
    Il2CppClass* godClass = Il2Cpp::FindClass("GodConstant");
    if (!godClass) return nullptr;

    return Il2Cpp::GetStaticFieldObject(godClass, "_instance");
}

Il2CppObject* Shared::TC() {
    Il2CppClass* tCClass = Il2Cpp::FindClass("PlanetJem.Roads.Traffic.TrafficCoordinator");
    if (!tCClass) return nullptr;

    return Il2Cpp::GetStaticFieldObject(tCClass, "instance_");
}

Il2CppObject* Shared::PlayerCar() {
    Il2CppObject* god = God();
    if (!god) return nullptr;

    return Il2Cpp::GetInstanceFieldObject(god, "playerCar");
}

Il2CppObject* Shared::CarData() {
    Il2CppObject* car = PlayerCar();
    if (!car) return nullptr;

    return Il2Cpp::GetInstanceFieldObject(car, "carData");
}

Il2CppObject* Shared::CarEngine() {
    Il2CppObject* carData = CarData();
    if (!carData) return nullptr;

    return Il2Cpp::GetInstanceFieldObject(carData, "carEngine");
}

Il2CppObject* Shared::CarLocal() {
    Il2CppObject* car = PlayerCar();
    if (!car) return nullptr;

    return Il2Cpp::GetInstanceFieldObject(car, "carLocal");
}

bool Shared::SetEnumField(Il2CppObject* instance, const char* fieldName, const Il2Cpp::EnumMember& member) {
    if (!instance) return false;
    return Il2Cpp::SetInstanceFieldObject(instance, fieldName, const_cast<uint8_t*>(member.raw.data()));
}

int Shared::FindEnumIndex(const std::vector<Il2Cpp::EnumMember>& options, const void* raw, size_t size) {
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i].raw.size() == size && memcmp(options[i].raw.data(), raw, size) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void Shared::SyncEnumIndex(Il2CppObject* instance, const char* fieldName, const std::vector<Il2Cpp::EnumMember>& options, int& index) {
    index = -1;
    if (!instance) return;

    int32_t value = 0;
    Il2Cpp::GetInstanceFieldValue(instance, fieldName, value);
    index = FindEnumIndex(options, &value, sizeof(value));
}
