#pragma once
#include "pch.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/main_thread.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

// Shared helpers used by more than one tab: waiting for managed state,
// destroying Unity objects, reading the current menu mode, resolving common
// game objects, and the enum/sequence utilities the tabs build on

class Shared {
public:
    // Mirrors UnityEngine.Color so it can be written into the game's field
    struct Colour {
        float r;
        float g;
        float b;
        float a;
    };

    // A per-module status line: a short message shown under a tab and mirrored
    // to the log. Each tab owns one instance
    class Status {
    public:
        explicit Status(const char* tag) : tag(tag) {}
        Status(const Status&) = delete;
        Status& operator=(const Status&) = delete;

        void Set(const std::string& message);
        std::string Get() const;
        void Render() const;

    private:
        const char* tag;
        mutable std::mutex mutex;
        std::string text;
    };

    // Steps work spread across frames (driving Unity coroutines): each step
    // returns true when finished; Fail() aborts the run. The timeout is a safety
    // net for a step that never completes
    class Sequence {
    public:
        using Step = std::function<bool()>;
        using Completion = std::function<void(bool ok, const std::string& message)>;

        void Start(std::vector<Step> steps, Completion onDone, std::chrono::milliseconds timeout);
        void Pump();
        void Fail(const std::string& message);
        bool Active() const { return active; }
        void Reset();

    private:
        void Finish(bool ok, const std::string& message);

        std::vector<Step> steps;
        size_t index = 0;
        Completion onDone;
        std::chrono::steady_clock::time_point deadline;
        bool active = false;
        std::string failure;
    };

    static void RenderLabel(const char* text);

    // Opens a tab and, when a save is loaded and the script thread is known,
    // returns true so the caller can fill it. Otherwise it renders the
    // "game not loaded" hint, closes the tab and returns false
    static bool BeginGameTab(const char* label);

    static void RenderEnumCombo(const char* label, const std::vector<Il2Cpp::EnumMember>& options, int selected, std::function<void(int)> onSelect);

    // Poll a condition until it becomes true or the timeout expires
    template <typename TPredicate>
    static bool WaitUntil(TPredicate predicate, int timeoutMs = 15000) {
        const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return predicate();
    }

    static Il2CppObject* WaitForResultField(Il2CppObject* instance, const char* fieldName);
    static void DestroyObject(Il2CppObject* object);

    // mainMenu.MainMenu_mode values, mirroring the enum nested in the mainMenu
    // MonoBehaviour and its private mainMenu_mode field
    enum class MainMenuMode : int32_t {
        Null = 0,
        MainMenu = 1,
        EnterName = 2,
        Paused = 3,
        Intro = 4,
        Leaderboard = 5,
    };

    // Reads mainMenu.mainMenu_mode. Returns false when the mainMenu object is not present
    static bool GetMainMenuMode(MainMenuMode& outMode);

    // True while the main menu is up
    static bool IsInMainMenu();
    static inline bool inMainMenu = false;

    static bool IsInGarage();
    static inline bool inGarage = false;

    // A game is loaded and the script thread has been identified, so game
    // actions can be queued through MainThread.
    static bool GameReady() {
        return MainThread::MainThreadKnown() && !inMainMenu;
    }

    // Shared game-object lookups
    static Il2CppObject* God();          // GodConstant._instance
    static Il2CppObject* TC();           // TrafficCoordinator.instance_
    static Il2CppObject* PlayerCar();    // GodConstant.playerCar
    static Il2CppObject* CarData();      // playerCar.carData
    static Il2CppObject* CarEngine();    // carData.carEngine
    static Il2CppObject* CarLocal();     // playerCar.carLocal

    // Enum helpers
    // Writes an enum literal (raw bytes from Il2Cpp::GetEnumMembers) into an instance field
    static bool SetEnumField(Il2CppObject* instance, const char* fieldName, const Il2Cpp::EnumMember& member);

    static int FindEnumIndex(const std::vector<Il2Cpp::EnumMember>& options, const void* raw, size_t size);
    static void SyncEnumIndex(Il2CppObject* instance, const char* fieldName, const std::vector<Il2Cpp::EnumMember>& options, int& index);
};
