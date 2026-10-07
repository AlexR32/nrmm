#include "pch.h"
#include "backends/d3d11.h"
#include "globals.h"
#include "menu.h"
#include "overlay.h"
#include "logger.h"
#include "input_block.h"
#include "config.h"
#include "nr/garage.h"
#include "nr/auction.h"
#include "nr/hooks.h"
#include "nr/player.h"
#include "nr/world.h"
#include "nr/current_car.h"
#include "nr/fixes.h"
#include "nr/music.h"

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

bool Menu::ShouldForwardToImGui(UINT msg) {
    switch (msg) {
    case WM_KEYDOWN: case WM_KEYUP:
    case WM_SYSKEYDOWN: case WM_SYSKEYUP:
    case WM_CHAR: case WM_SYSCHAR:
    case WM_MOUSEMOVE: case WM_NCMOUSEMOVE: case WM_MOUSELEAVE:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
    case WM_SETFOCUS: case WM_KILLFOCUS:
    case WM_ACTIVATE: case WM_ACTIVATEAPP:
    case WM_SETCURSOR:
        return true;
    default:
        return false;
    }
}

bool Menu::ShouldSwallow(UINT msg) {
    switch (msg) {
    case WM_KEYDOWN: case WM_KEYUP:
    case WM_SYSKEYDOWN: case WM_SYSKEYUP:
    case WM_CHAR: case WM_SYSCHAR:
    case WM_MOUSEMOVE: case WM_MOUSELEAVE:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
        return true;
    default:
        return false;
    }
}

bool Menu::IsKeyboardMessage(UINT msg) {
    switch (msg) {
    case WM_KEYDOWN: case WM_KEYUP:
    case WM_SYSKEYDOWN: case WM_SYSKEYUP:
    case WM_CHAR: case WM_SYSCHAR:
        return true;
    default:
        return false;
    }
}

std::string Menu::KeyName(int vk) {
    switch (vk) {
    case 0: return "NONE";
    case VK_INSERT: return "INSERT";
    case VK_DELETE: return "DELETE";
    case VK_HOME: return "HOME";
    case VK_END: return "END";
    case VK_PRIOR: return "PAGE UP";
    case VK_NEXT: return "PAGE DOWN";
    case VK_ESCAPE: return "ESCAPE";
    case VK_TAB: return "TAB";
    case VK_SPACE: return "SPACE";
    case VK_RETURN: return "ENTER";
    case VK_BACK: return "BACKSPACE";
    case VK_CAPITAL: return "CAPS LOCK";
    case VK_LEFT: return "LEFT";
    case VK_RIGHT: return "RIGHT";
    case VK_UP: return "UP";
    case VK_DOWN: return "DOWN";
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return "SHIFT";
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return "CTRL";
    case VK_MENU: case VK_LMENU: case VK_RMENU: return "ALT";
    default: break;
    }

    if (vk >= VK_F1 && vk <= VK_F24) return "F" + std::to_string(vk - VK_F1 + 1);
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) return std::string(1, static_cast<char>(vk));

    const UINT scan = MapVirtualKeyA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
    char buffer[64]{};
    const LONG lparam = static_cast<LONG>(scan << 16);
    if (GetKeyNameTextA(lparam, buffer, static_cast<int>(sizeof(buffer))) > 0) return buffer;

    return "VK " + std::to_string(vk);
}

void Menu::RenderKeybind(const char* label, int key, int target) {
    const bool capturing = captureTarget.load(std::memory_order_relaxed) == target;
    const std::string value = capturing ? "Press a key..." : KeyName(key);

    ImGui::PushID(label);
    if (ImGui::Button(value.c_str())) {
        captureTarget.store(capturing ? 0 : target, std::memory_order_relaxed);
    }
    ImGui::SetItemTooltip(capturing ? "Press a key (ESC cancels)" : "Click, then press a key to rebind");
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
}

void Menu::InitStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = ImGui::GetStyle().Colors;

    // Properties
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.5f);

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
    struct TabEntry {
        const char* label;
        void (*render)();
    };
    const TabEntry tabs[] = {
        {"Settings", RenderSettingsTab},
        {"Player", Player::RenderTab},
        {"Current Car", CurrentCar::RenderTab},
        {"World", World::RenderTab},
        {"Garage", Garage::RenderTab},
        {"Auction", Auction::RenderTab},
        {"Music", Music::RenderTab},
        {"Fixes", Fixes::RenderTab},
    };
    const int tabCount = IM_ARRAYSIZE(tabs);
    if (selectedTab < 0 || selectedTab >= tabCount) selectedTab = 0;

    ImGui::SetNextWindowSizeConstraints(ImVec2(kWidth, 0.0f), ImVec2(kWidth, FLT_MAX));
    if (ImGui::Begin(kWindowName, 0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        float sidebarWidth = 0.0f;
        for (int i = 0; i < tabCount; ++i) {
            sidebarWidth = ImMax(sidebarWidth, ImGui::CalcTextSize(tabs[i].label).x);
        }
        sidebarWidth += ImGui::GetStyle().CellPadding.x * 2.0f;

        if (ImGui::BeginTable("menu_layout", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("nav", ImGuiTableColumnFlags_WidthFixed, sidebarWidth);
            ImGui::TableSetupColumn("content", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            for (int i = 0; i < tabCount; ++i) {
                const ImVec2 size(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight());
                if (ImGui::Selectable(tabs[i].label, selectedTab == i, 0, size)) {
                    selectedTab = i;
                }
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::PushID(selectedTab);
            tabs[selectedTab].render();
            ImGui::PopID();

            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void Menu::RenderSettingsTab() {
    bool blockKeyboard = Config::BlockKeyboard();
    if (ImGui::Checkbox("Block Keyboard", &blockKeyboard)) {
        Config::SetBlockKeyboard(blockKeyboard);
    }

    bool showConsole = Config::DebugConsole();
    if (ImGui::Checkbox("Debug Console", &showConsole)) {
        Config::SetDebugConsole(showConsole);
    }

    if (ImGui::Button("UNLOAD DLL")) {
        D3D11Hook::shuttingDown.store(true, std::memory_order_release);
        g_Running.store(false, std::memory_order_release);
    }

    RenderKeybind("Menu Toggle", Config::ToggleKey(), 1);
    RenderKeybind("Unload DLL", Config::UnloadKey(), 2);
}

void Menu::Render() {
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = visible;

    // Managed code must run on the game's script thread; the GodConstant.Update
    // hook binds MainThread and pumps queued actions. Until it is up
    // Shared::GameReady() is false and no tab runs any game action

    ImVec2 topCenter = ImVec2({io.DisplaySize.x * 0.5f, 0.0f});
    Overlay::TextOutlinedCentered("NRMM | alexr32 @ discord.com", topCenter);

    Logger::Render();

    if (!visible) return;

    RenderMainWindow();
}

LRESULT Menu::HandleInput(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    const int capture = captureTarget.load(std::memory_order_relaxed);
    if (capture != 0 && (uMsg == WM_KEYUP || uMsg == WM_SYSKEYUP)) {
        const int vk = static_cast<int>(wParam);
        if (vk != VK_ESCAPE) {
            if (capture == 1) Config::SetToggleKey(vk);
            else Config::SetUnloadKey(vk);
        }
        captureTarget.store(0, std::memory_order_relaxed);
        return TRUE;
    }

    if ((uMsg == WM_KEYUP || uMsg == WM_SYSKEYUP) && wParam == static_cast<WPARAM>(Config::ToggleKey())) {
        const bool show = !visible.load(std::memory_order_relaxed);
        visible.store(show, std::memory_order_relaxed);

        if (show) {
            InputBlock::Suppress();

            POINT target{};
            bool haveTarget = false;
            if (hasSavedCursor) {
                target = savedCursor;
                haveTarget = true;
            } else if (hWnd && GetCursorPos(&target)) {
                haveTarget = true;
            }

            if (haveTarget && hWnd) {
                InputBlock::WarpCursor(target.x, target.y);

                POINT client = target;
                if (ScreenToClient(hWnd, &client)) {
                    std::lock_guard<std::mutex> lock(inputMutex);
                    if (inputQueue.size() >= kMaxQueuedInput) inputQueue.pop_front();
                    inputQueue.push_back({hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(client.x, client.y)});
                }
            }
        } else {
            if (hWnd && GetCursorPos(&savedCursor)) hasSavedCursor = true;
            InputBlock::Release();
        }

        return TRUE;
    }

    if ((uMsg == WM_KEYUP || uMsg == WM_SYSKEYUP) && wParam == static_cast<WPARAM>(Config::UnloadKey())) {
        D3D11Hook::shuttingDown.store(true, std::memory_order_release);
        g_Running.store(false, std::memory_order_release);
        return TRUE;
    }

    if (!visible.load(std::memory_order_relaxed)) return FALSE;

    if (uMsg == WM_INPUT) {
        RAWINPUT raw{};
        UINT size = sizeof(raw);
        const UINT result = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));
        const bool isHid = result != static_cast<UINT>(-1) && result >= sizeof(RAWINPUTHEADER) && raw.header.dwType == RIM_TYPEHID;

        if (!isHid) {
            DefWindowProc(hWnd, uMsg, wParam, lParam);
            return TRUE;
        }
    }

    if (ShouldForwardToImGui(uMsg)) {
        std::lock_guard<std::mutex> lock(inputMutex);
        if (inputQueue.size() >= kMaxQueuedInput) inputQueue.pop_front();
        inputQueue.push_back({hWnd, uMsg, wParam, lParam});
    }

    bool swallow = ShouldSwallow(uMsg);
    if (swallow && IsKeyboardMessage(uMsg) && !InputBlock::KeyboardBlock()) swallow = false;

    return swallow ? TRUE : FALSE;
}

void Menu::PumpInput() {
    InputBlock::BindRenderThread();

    std::deque<InputMessage> pending;
    {
        std::lock_guard<std::mutex> lock(inputMutex);
        if (inputQueue.empty()) return;
        pending.swap(inputQueue);
    }

    for (const InputMessage& message : pending) {
        ImGui_ImplWin32_WndProcHandler(message.hWnd, message.msg, message.wParam, message.lParam);
    }
}

void Menu::Initialize() {
    if (initialized) return;
    D3D11Hook::RegisterRenderCallback(Render);
    D3D11Hook::RegisterWndProcCallback(HandleInput);
    D3D11Hook::RegisterNewFrameCallback(PumpInput);

    ImGui::GetIO().IniFilename = NULL;
    InitStyle();

    Config::Load();
    InputBlock::Install();

    if (visible.load(std::memory_order_relaxed)) {
        InputBlock::Suppress();
    }

    initialized = true;
}

void Menu::Shutdown() {
    if (!initialized) return;
    Hooks::Remove();
    InputBlock::Uninstall();
    initialized = false;
}
