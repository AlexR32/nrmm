#pragma once

#include "framework.h"
#include "resource.h"
#include "theme.h"

// Owns the single bootstrapper window and all of its custom-drawn controls.
class App {
public:
    explicit App(HINSTANCE instance) : instance_(instance) {}

    bool Initialize(int showCommand);
    int Run();

private:
    static LRESULT CALLBACK WindowProcStatic(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ButtonProcStatic(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData);

    LRESULT WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void CreateControls();
    void Layout();
    void ApplyTheme();
    void ApplyTitleBarTheme();
    void DrawButton(const DRAWITEMSTRUCT* item);
    void DrawStatus(const DRAWITEMSTRUCT* item);
    void OnCommand(int id);
    void OnInject();
    void OnDecrypt();
    void OnEncrypt();
    void SetHot(int controlId, bool hot);
    void SetStatus(const std::wstring& text, bool error);
    int Scale(int value) const;
    HFONT MakeFont(int pointSize, bool bold) const;

    static std::wstring ExecutableDirectory();
    static std::wstring GameSaveDirectory();
    static bool FileExists(const std::wstring& path);
    static bool ReadFileBytes(const std::wstring& path, std::vector<uint8_t>& data, std::wstring& error);
    static bool WriteFileBytes(const std::wstring& path, const std::vector<uint8_t>& data, std::wstring& error);
    static std::wstring FormatBytes(size_t bytes);

    static constexpr const wchar_t* kWindowClass = L"NRMMBootstrapperWindow";
    static constexpr const wchar_t* kWindowTitle = L"NRMM Bootstrapper";
    static constexpr const wchar_t* kGameProcess = L"NIGHT-RUNNERS PRIVATE ALPHA.exe";
    static constexpr const wchar_t* kReleaseDll = L"nrmm.dll";
    static constexpr const wchar_t* kDebugDll = L"nrmm-debug.dll";

    static constexpr char kSavePassword[] = "918&%gW]^_gI";

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;

    HWND header_ = nullptr;
    HWND btnInject_ = nullptr;
    HWND btnDecrypt_ = nullptr;
    HWND btnEncrypt_ = nullptr;
    HWND status_ = nullptr;

    HFONT headerFont_ = nullptr;
    HFONT bodyFont_ = nullptr;
    HBRUSH backgroundBrush_ = nullptr;

    const Theme* theme_ = nullptr;
    int dpi_ = 96;
    int hotId_ = 0;
    bool statusError_ = false;
};
