#include "pch.h"

#include "backends/d3d11.h"
#include "core/logger.h"
#include "core/menu.h"
#include "il2cpp/il2cpp.h"
#include "nr/hooks.h"

HMODULE g_hModule = nullptr;
std::atomic_bool g_Running = false;
std::wstring g_DllPath = L"";
static HANDLE g_imguiThread = nullptr;

static std::wstring GetDllPath(HMODULE hModule) {
    WCHAR buffer[MAX_PATH]{};
    GetModuleFileNameW(hModule, buffer, MAX_PATH);
    std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
    return std::wstring(buffer).substr(0, pos);
}

static DWORD WINAPI WaitForImGuiThread(LPVOID) {
    Logger::Log("Waiting for the Direct3D Present hook...");

    while (g_Running.load(std::memory_order_acquire) && !D3D11Hook::initialized)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    if (!g_Running.load(std::memory_order_acquire)) return 0;

    Logger::Log("Direct3D initialized");
    Logger::Log("Initializing menu...");

    Menu::Initialize();

    Logger::Log("Menu initialized successfully");
    return 0;
}

static DWORD WINAPI MainThread(LPVOID hModule) {
    g_hModule = static_cast<HMODULE>(hModule);

    if (MH_Initialize() != MH_OK) {
        MessageBoxA(nullptr, "An error occurred while initializing MinHook", "NRMM - Error", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
        FreeLibraryAndExitThread(g_hModule, EXIT_SUCCESS);
        return EXIT_SUCCESS;
    }

    if (!Il2Cpp::Initialize()) {
        MessageBoxA(nullptr, "An error occurred while initializing il2cpp", "NRMM - Error", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
        MH_Uninitialize();
        FreeLibraryAndExitThread(g_hModule, EXIT_SUCCESS);
        return EXIT_SUCCESS;
    }

    if (!D3D11Hook::Initialize()) {
        MessageBoxA(nullptr, "An error occurred while hooking Direct3D", "NRMM - Error", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
        MH_Uninitialize();
        FreeLibraryAndExitThread(g_hModule, EXIT_SUCCESS);
        return EXIT_SUCCESS;
    }

    g_DllPath = GetDllPath(g_hModule);

    Logger::Initialize(g_DllPath, L"nrmm");
    Logger::SetTitle("[NRMM] Debug Console");
    Logger::SetVisibility(false);

    g_Running.store(true, std::memory_order_release);

    g_imguiThread = CreateThread(nullptr, 0, WaitForImGuiThread, nullptr, 0, nullptr);

    if (!g_imguiThread) {
        Logger::Log("Failed to create ImGui waiting thread");
        g_Running.store(false, std::memory_order_release);
    }

    // Install the script-thread hook from this worker loop rather than from the
    // render callback, keeping il2cpp reflection and MinHook off the Present thread
    while (g_Running.load(std::memory_order_acquire)) {
        Hooks::EnsureInstalled();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // EXIT
    // g_Running is already false, so the menu thread leaves its wait and, if it
    // got that far, finishes Menu::Initialize. Joining it here keeps Shutdown
    // from racing an in-progress Initialize and leaking the input hooks.
    if (g_imguiThread) {
        WaitForSingleObject(g_imguiThread, 10000);
        CloseHandle(g_imguiThread);
        g_imguiThread = nullptr;
    }

    // Stop the render pipeline first so no detour can reach the mod after the
    // script-thread hook and input hooks are removed
    D3D11Hook::Cleanup();
    Menu::Shutdown();
    MH_Uninitialize();
    Logger::Cleanup();
    FreeLibraryAndExitThread(g_hModule, EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwCallReason, LPVOID lpReserved) {
    UNREFERENCED_PARAMETER(lpReserved);

    if (dwCallReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);

        HANDLE hThread = CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
        if (hThread == nullptr) return FALSE;
        CloseHandle(hThread);
    }

    return TRUE;
}
