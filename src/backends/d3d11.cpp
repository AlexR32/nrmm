#include "pch.h"
#include "d3d11.h"

void D3D11Hook::InitializeImGui() {
    if (!imguiInitialized) {
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(hWnd);
        ImGui_ImplDX11_Init(device, context);
        oWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hkWndProc)));
        imguiInitialized = true;
    }
}

void D3D11Hook::ShutdownImGui() {
    if (imguiInitialized) {
        if (IsWindow(hWnd))
            SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        imguiInitialized = false;
    }
}

void D3D11Hook::CreateRenderTarget(IDXGISwapChain* pSwapChain) {
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (SUCCEEDED(pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)))) {
        HRESULT hr = device->CreateRenderTargetView(pBackBuffer, nullptr, &renderTargetView);
        if (FAILED(hr))
            renderTargetView = nullptr;
        pBackBuffer->Release();
    }
}

void D3D11Hook::CleanupRenderTarget() {
    if (renderTargetView) {
        renderTargetView->Release();
        renderTargetView = nullptr;
    }
}

void D3D11Hook::ReleaseDummyObjects() {
    if (dummySwapChain) { dummySwapChain->Release(); dummySwapChain = nullptr; }
    if (dummyContext) { dummyContext->Release(); dummyContext = nullptr; }
    if (dummyDevice) { dummyDevice->Release(); dummyDevice = nullptr; }
}

void D3D11Hook::ReleaseGameObjects() {
    if (gameObjectsAcquired) {
        if (context) { context->Release(); context = nullptr; }
        if (device) { device->Release(); device = nullptr; }
        gameObjectsAcquired = false;
    }
}

bool D3D11Hook::GetDeviceAndSwapChain() {
    WNDCLASS wc = {};
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"D3D11Class";

    if (!RegisterClass(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
    }

    HWND hWnd = CreateWindowEx(0, wc.lpszClassName, L"D3D11", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
    if (!hWnd) return false;

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.Width = 100;
    scd.BufferDesc.Height = 100;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hWnd;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = TRUE;

    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &scd, &dummySwapChain, &dummyDevice, nullptr, &dummyContext);

    swapChain = dummySwapChain;
    device = dummyDevice;
    context = dummyContext;

    DestroyWindow(hWnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return SUCCEEDED(hr);
}

void D3D11Hook::InitializeHooks() {
    uintptr_t* vtable = *reinterpret_cast<uintptr_t**>(dummySwapChain);
    pPresent = reinterpret_cast<void*>(vtable[8]);
    pResizeBuffers = reinterpret_cast<void*>(vtable[13]);

    MH_CreateHook(pPresent, reinterpret_cast<void*>(&hkPresent), reinterpret_cast<void**>(&oPresent));
    MH_CreateHook(pResizeBuffers, reinterpret_cast<void*>(&hkResizeBuffers), reinterpret_cast<void**>(&oResizeBuffers));
    MH_EnableHook(pPresent);
    MH_EnableHook(pResizeBuffers);
}

LRESULT CALLBACK D3D11Hook::hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (wndProcCallback) {
        if (wndProcCallback(hWnd, uMsg, wParam, lParam))
            return TRUE;
    }

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

HRESULT STDMETHODCALLTYPE D3D11Hook::hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!initialized) {
        std::lock_guard<std::mutex> lock(initMutex);
        if (!initialized) {
            if (FAILED(pSwapChain->GetDevice(IID_PPV_ARGS(&device))))
                return oPresent(pSwapChain, SyncInterval, Flags);

            device->GetImmediateContext(&context);
            gameObjectsAcquired = true;

            ReleaseDummyObjects();

            DXGI_SWAP_CHAIN_DESC desc{};
            if (FAILED(pSwapChain->GetDesc(&desc)))
                return oPresent(pSwapChain, SyncInterval, Flags);

            hWnd = desc.OutputWindow;
            swapChain = pSwapChain;

            CreateRenderTarget(pSwapChain);
            InitializeImGui();

            initialized = true;
        }
    }

    if (!renderTargetView)
        return oPresent(pSwapChain, SyncInterval, Flags);

    ID3D11RenderTargetView* oldRTV = nullptr;
    ID3D11DepthStencilView* oldDSV = nullptr;
    context->OMGetRenderTargets(1, &oldRTV, &oldDSV);

    context->OMSetRenderTargets(1, &renderTargetView, nullptr);

    if (newFrameCallback)
        newFrameCallback();

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (renderCallback)
        renderCallback();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    context->OMSetRenderTargets(1, &oldRTV, oldDSV);
    if (oldRTV) oldRTV->Release();
    if (oldDSV) oldDSV->Release();

    HRESULT hr = oPresent(pSwapChain, SyncInterval, Flags);

    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        initialized = false;
        ShutdownImGui();
        CleanupRenderTarget();
        ReleaseGameObjects();
        swapChain = nullptr;
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE D3D11Hook::hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
    ImGui_ImplDX11_InvalidateDeviceObjects();
    CleanupRenderTarget();

    HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    if (FAILED(hr))
        return hr;

    CreateRenderTarget(pSwapChain);
    ImGui_ImplDX11_CreateDeviceObjects();

    return hr;
}

bool D3D11Hook::Initialize() {
    if (!GetDeviceAndSwapChain()) return false;
    InitializeHooks();
    return true;
}

void D3D11Hook::Cleanup() {
    MH_DisableHook(pPresent);
    MH_DisableHook(pResizeBuffers);
    MH_RemoveHook(pPresent);
    MH_RemoveHook(pResizeBuffers);

    ShutdownImGui();
    CleanupRenderTarget();
    ReleaseGameObjects();
    ReleaseDummyObjects();

    swapChain = nullptr;
    initialized = false;
}
