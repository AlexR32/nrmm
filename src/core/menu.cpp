#include "pch.h"
#include "backends/d3d11.h"
#include "globals.h"
#include "menu.h"
#include "overlay.h"
#include "logger.h"
#include "cursor.h"
#include "nr/garage.h"
#include "nr/auction.h"
#include "nr/hooks.h"
#include "nr/player.h"
#include "nr/world.h"
#include "nr/current_car.h"
#include "nr/fixes.h"

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void Menu::InitStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = ImGui::GetStyle().Colors;

    // Properties
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);

    style.ChildRounding = 0;
    style.FrameRounding = 0;
    style.GrabRounding = 0;
    style.PopupRounding = 0;
    style.ScrollbarRounding = 0;
    style.TabRounding = 0;
    style.WindowRounding = 0;

    // Colors
    // Windows
    colors[ImGuiCol_WindowBg] = ImVec4{0.1f, 0.1f, 0.13f, 1.0f};
    colors[ImGuiCol_MenuBarBg] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};

    // Border
    colors[ImGuiCol_Border] = ImVec4{0.44f, 0.37f, 0.61f, 0.29f};
    colors[ImGuiCol_BorderShadow] = ImVec4{0.0f, 0.0f, 0.0f, 0.24f};

    // Text
    colors[ImGuiCol_Text] = ImVec4{1.0f, 1.0f, 1.0f, 1.0f};
    colors[ImGuiCol_TextDisabled] = ImVec4{0.5f, 0.5f, 0.5f, 1.0f};

    // Headers
    colors[ImGuiCol_Header] = ImVec4{0.13f, 0.13f, 0.17f, 1.0f};
    colors[ImGuiCol_HeaderHovered] = ImVec4{0.19f, 0.2f, 0.25f, 1.0f};
    colors[ImGuiCol_HeaderActive] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};

    // Buttons
    colors[ImGuiCol_Button] = ImVec4{0.13f, 0.13f, 0.17f, 1.0f};
    colors[ImGuiCol_ButtonHovered] = ImVec4{0.19f, 0.2f, 0.25f, 1.0f};
    colors[ImGuiCol_ButtonActive] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};
    colors[ImGuiCol_CheckMark] = ImVec4{0.74f, 0.58f, 0.98f, 1.0f};

    // Popups
    colors[ImGuiCol_PopupBg] = ImVec4{0.1f, 0.1f, 0.13f, 0.92f};

    // Slider
    colors[ImGuiCol_SliderGrab] = ImVec4{0.44f, 0.37f, 0.61f, 0.54f};
    colors[ImGuiCol_SliderGrabActive] = ImVec4{0.74f, 0.58f, 0.98f, 0.54f};

    // Frame BG
    colors[ImGuiCol_FrameBg] = ImVec4{0.13f, 0.13f, 0.17f, 1.0f};
    colors[ImGuiCol_FrameBgHovered] = ImVec4{0.19f, 0.2f, 0.25f, 1.0f};
    colors[ImGuiCol_FrameBgActive] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};
    colors[ImGuiCol_TabHovered] = ImVec4{0.24f, 0.24f, 0.32f, 1.0f};
    colors[ImGuiCol_TabSelected] = ImVec4{0.2f, 0.22f, 0.27f, 1.0f};
    colors[ImGuiCol_TabDimmed] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};
    colors[ImGuiCol_TabDimmedSelected] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};

    // Title
    colors[ImGuiCol_TitleBg] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};
    colors[ImGuiCol_TitleBgActive] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = ImVec4{0.1f, 0.1f, 0.13f, 1.0f};
    colors[ImGuiCol_ScrollbarGrab] = ImVec4{0.16f, 0.16f, 0.21f, 1.0f};
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4{0.19f, 0.2f, 0.25f, 1.0f};
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4{0.24f, 0.24f, 0.32f, 1.0f};

    // Separator
    colors[ImGuiCol_Separator] = ImVec4{0.44f, 0.37f, 0.61f, 1.0f};
    colors[ImGuiCol_SeparatorHovered] = ImVec4{0.74f, 0.58f, 0.98f, 1.0f};
    colors[ImGuiCol_SeparatorActive] = ImVec4{0.84f, 0.58f, 1.0f, 1.0f};

    // Resize Grip
    colors[ImGuiCol_ResizeGrip] = ImVec4{0.44f, 0.37f, 0.61f, 0.29f};
    colors[ImGuiCol_ResizeGripHovered] = ImVec4{0.74f, 0.58f, 0.98f, 0.29f};
    colors[ImGuiCol_ResizeGripActive] = ImVec4{0.84f, 0.58f, 1.0f, 0.29f};
}

void Menu::RenderMainWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(270.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::Begin("NIGHT RUNNERS MOD MENU", 0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        if (ImGui::BeginTabBar("main_tab_bar")) {
            RenderSettingsTab();
            Player::RenderTab();
            World::RenderTab();
            CurrentCar::RenderTab();
            Fixes::RenderTab();
            Garage::RenderTab();
            Auction::RenderTab();

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void Menu::RenderSettingsTab() {
    if (!ImGui::BeginTabItem("Settings")) return;

    if (ImGui::Button("UNHOOK DLL")) {
        g_Running.store(false, std::memory_order_release);
    }

    ImGui::SameLine();

    bool showConsole = Logger::GetVisibility();
    if (ImGui::Checkbox("Show Console", &showConsole)) {
        Logger::SetVisibility(showConsole);
    }

    ImGui::EndTabItem();
}

void Menu::Render() {
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = visible;

    // Managed code must run on the game's script thread; the GodConstant.Update
    // hook binds MainThread and pumps queued actions. Until it is up
    // Shared::GameReady() is false and no tab runs any game action
    Hooks::EnsureInstalled();

    ImVec2 topCenter = ImVec2({io.DisplaySize.x * 0.5f, 0.0f});
    Overlay::TextOutlinedCentered("NRMM | alexr32 @ discord.com", topCenter);

    if (!visible) return;

    RenderMainWindow();
}

LRESULT Menu::HandleInput(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYUP && wParam == VK_INSERT) {
        visible = !visible;
        if (visible) Cursor::ReleaseClip();
        return TRUE;
    }

    if (uMsg == WM_KEYUP && wParam == VK_DELETE) {
        g_Running.store(false, std::memory_order_release);
        return TRUE;
    }

    if (visible) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
            return TRUE;

        switch (uMsg) {
        case WM_KEYDOWN: case WM_KEYUP:
        case WM_SYSKEYDOWN: case WM_SYSKEYUP:
        case WM_CHAR: case WM_SYSCHAR:
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
        case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
        case WM_INPUT:
            return TRUE;
        }
    }

    return FALSE;
}

void Menu::Initialize() {
    if (initialized) return;
    D3D11Hook::RegisterRenderCallback(Render);
    D3D11Hook::RegisterWndProcCallback(HandleInput);

    ImGui::GetIO().IniFilename = NULL;
    InitStyle();

    Cursor::Install();

    initialized = true;
}

void Menu::Shutdown() {
    if (!initialized) return;
    Hooks::Remove();
    Cursor::Uninstall();
    initialized = false;
}
