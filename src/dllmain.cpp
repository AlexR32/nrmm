#include "pch.h"

#include "backends/d3d11.h"
#include "core/logger.h"
#include "core/menu.h"
#include "il2cpp/il2cpp.h"

HMODULE g_hModule = nullptr;
std::atomic_bool g_Running = false;
std::string g_DllPath = "";

static std::string GetDllPath(HMODULE hModule) {
    CHAR buffer[MAX_PATH]{};
    GetModuleFileNameA(hModule, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("\\/");
    return std::string(buffer).substr(0, pos);
}

static DWORD WINAPI WaitForImGuiThread(LPVOID) {
    while (!D3D11Hook::initialized) {
        Logger::Log("Waiting for Direct3D hook...");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    Logger::Log("Direct3D initialized");
    Logger::Log("Direct3D hook initialized successfully");
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
        FreeLibraryAndExitThread(g_hModule, EXIT_SUCCESS);
        return EXIT_SUCCESS;
    }

    if (!D3D11Hook::Initialize()) {
        MessageBoxA(nullptr, "An error occurred while hooking Direct3D", "NRMM - Error", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
        FreeLibraryAndExitThread(g_hModule, EXIT_SUCCESS);
        return EXIT_SUCCESS;
    }

    g_DllPath = GetDllPath(g_hModule);

    Logger::Initialize(g_DllPath, "nrmm");
    Logger::SetTitle("[NRMM] Debug Console");
    Logger::SetVisibility(false);

    g_Running.store(true, std::memory_order_release);

    HANDLE imguiThread = CreateThread(nullptr, 0, WaitForImGuiThread, nullptr, 0, nullptr);

    if (imguiThread) {
        CloseHandle(imguiThread);
    } else {
        Logger::Log("Failed to create ImGui waiting thread");
        g_Running.store(false, std::memory_order_release);
    }

    while (g_Running.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // EXIT
    Menu::Shutdown();
    D3D11Hook::Cleanup();
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
