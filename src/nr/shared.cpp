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

Il2CppObject* Shared::GetTransformParent(Il2CppObject* transform) {
    if (!transform) return nullptr;

    Il2CppImage* coreModule = Il2Cpp::GetImage("UnityEngine.CoreModule");
    Il2CppClass* transformClass = Il2Cpp::FindClass("UnityEngine.Transform", coreModule);
    const MethodInfo* getParent = transformClass ? Il2Cpp::GetMethod(transformClass, "get_parent", 0) : nullptr;
    return getParent ? Il2Cpp::Invoke(getParent, transform, nullptr) : nullptr;
}

Il2CppObject* Shared::FindDescendantByNamePrefix(Il2CppObject* transform, const char* prefix) {
    if (!transform || !prefix) return nullptr;

    Il2CppImage* coreModule = Il2Cpp::GetImage("UnityEngine.CoreModule");
    Il2CppClass* transformClass = Il2Cpp::FindClass("UnityEngine.Transform", coreModule);
    Il2CppClass* objectClass = Il2Cpp::FindClass("UnityEngine.Object", coreModule);
    const MethodInfo* getChildCount = transformClass ? Il2Cpp::GetMethod(transformClass, "get_childCount", 0) : nullptr;
    const MethodInfo* getChild = transformClass ? Il2Cpp::GetMethod(transformClass, "GetChild", 1) : nullptr;
    const MethodInfo* getName = objectClass ? Il2Cpp::GetMethod(objectClass, "get_name", 0) : nullptr;
    if (!getChildCount || !getChild || !getName) return nullptr;

    std::vector<Il2CppObject*> children;
    const int count = Il2Cpp::UnboxInt32(Il2Cpp::Invoke(getChildCount, transform, nullptr));
    children.reserve(count > 0 ? static_cast<size_t>(count) : 0);
    for (int i = 0; i < count; ++i) {
        int index = i;
        void* args[1] = {&index};
        if (Il2CppObject* child = Il2Cpp::Invoke(getChild, transform, args)) children.push_back(child);
    }

    for (Il2CppObject* child : children) {
        Il2CppObject* nameObject = Il2Cpp::Invoke(getName, child, nullptr);
        const std::string name = nameObject ? Il2Cpp::StringToUtf8(reinterpret_cast<Il2CppString*>(nameObject)) : "";
        if (name.rfind(prefix, 0) == 0) return child;
    }

    for (Il2CppObject* child : children) {
        if (Il2CppObject* found = FindDescendantByNamePrefix(child, prefix)) return found;
    }

    return nullptr;
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
