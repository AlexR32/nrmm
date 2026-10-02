#pragma once

#include <windows.h>
#include <functional>
#include <mutex>
#include <atomic>
#include <d3d11.h>

class D3D11Hook {
public:
    using RenderCallback = std::function<void()>;
    using WndProcCallback = std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)>;
    using NewFrameCallback = std::function<void()>;

    static bool Initialize();
    static void Cleanup();

    // Read by the initialization-wait thread while Present writes it
    static inline std::atomic_bool initialized{ false };
    static inline bool imguiInitialized = false;
    static inline std::atomic_bool shuttingDown{ false };

    static inline IDXGISwapChain* swapChain = nullptr;
    static inline ID3D11Device* device = nullptr;
    static inline ID3D11DeviceContext* context = nullptr;
    static inline ID3D11RenderTargetView* renderTargetView = nullptr;

    static void RegisterRenderCallback(RenderCallback callback) { renderCallback = callback; }
    static void RegisterWndProcCallback(WndProcCallback callback) { wndProcCallback = callback; }
    static void RegisterNewFrameCallback(NewFrameCallback callback) { newFrameCallback = callback; }
private:
    static inline HWND hWnd = nullptr;
    static inline std::atomic<WNDPROC> oWndProc{ nullptr };

    static inline IDXGISwapChain* dummySwapChain = nullptr;
    static inline ID3D11Device* dummyDevice = nullptr;
    static inline ID3D11DeviceContext* dummyContext = nullptr;

    static inline RenderCallback renderCallback = nullptr;
    static inline WndProcCallback wndProcCallback = nullptr;
    static inline NewFrameCallback newFrameCallback = nullptr;

    static inline std::mutex initMutex;
    static inline bool gameObjectsAcquired = false;

    // Set before the present/resize hooks are disabled. In-flight hooks check it
    // so they stop touching ImGui, and Cleanup waits for inFlight to reach zero
    // before tearing the context down. Without this a detour running on the
    // render thread could use a destroyed ImGui context during unload.
    static inline std::atomic_bool unloading{ false };
    static inline std::atomic_int inFlight{ 0 };

    struct HookScope {
        HookScope() { inFlight.fetch_add(1, std::memory_order_acq_rel); }
        ~HookScope() { inFlight.fetch_sub(1, std::memory_order_acq_rel); }
    };

    static void ReleaseDummyObjects();
    static void ReleaseGameObjects();

    static void InitializeImGui();
    static void ShutdownImGui();

    static void CreateRenderTarget(IDXGISwapChain* pSwapChain);
    static void CleanupRenderTarget();

    static bool GetDeviceAndSwapChain();
    static bool InitializeHooks();

    static LRESULT CALLBACK hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static HRESULT STDMETHODCALLTYPE hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
    static inline decltype(&hkPresent) oPresent = nullptr;
    static inline void* pPresent = nullptr;

    static HRESULT STDMETHODCALLTYPE hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
    static inline decltype(&hkResizeBuffers) oResizeBuffers = nullptr;
    static inline void* pResizeBuffers = nullptr;
};
