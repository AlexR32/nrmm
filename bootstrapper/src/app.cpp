#include "app.h"

#include "injector.h"
#include "save_crypto.h"

#include <algorithm>
#include <cwchar>

std::wstring App::ExecutableDirectory() {
    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return L"";
    std::wstring full(path, length);
    const size_t slash = full.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : full.substr(0, slash);
}

std::wstring App::GameSaveDirectory() {
    PWSTR base = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppDataLow, 0, nullptr, &base))) return L"";
    std::wstring directory(base);
    CoTaskMemFree(base);
    directory += L"\\PLANET JEM SOFTWARE\\NIGHT-RUNNERS PRIVATE ALPHA";
    return directory;
}

bool App::FileExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool App::ReadFileBytes(const std::wstring& path, std::vector<uint8_t>& data, std::wstring& error) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = L"Failed to open \"" + path + L"\".";
        return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || static_cast<uint64_t>(size.QuadPart) > (1ull << 30)) {
        error = L"Failed to read the size of \"" + path + L"\".";
        CloseHandle(file);
        return false;
    }

    data.resize(static_cast<size_t>(size.QuadPart));
    size_t read = 0;
    bool ok = true;
    while (read < data.size()) {
        DWORD chunk = 0;
        const DWORD want = static_cast<DWORD>(std::min<size_t>(data.size() - read, 1u << 20));
        if (!ReadFile(file, data.data() + read, want, &chunk, nullptr) || chunk == 0) {
            ok = false;
            break;
        }
        read += chunk;
    }

    CloseHandle(file);
    if (!ok) {
        error = L"Failed to read \"" + path + L"\". It may be locked by the game.";
        return false;
    }
    return true;
}

bool App::WriteFileBytes(const std::wstring& path, const std::vector<uint8_t>& data, std::wstring& error) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = L"Failed to write \"" + path + L"\". It may be locked by the game.";
        return false;
    }

    size_t written = 0;
    bool ok = true;
    while (written < data.size()) {
        DWORD chunk = 0;
        const DWORD want = static_cast<DWORD>(std::min<size_t>(data.size() - written, 1u << 20));
        if (!WriteFile(file, data.data() + written, want, &chunk, nullptr) || chunk == 0) {
            ok = false;
            break;
        }
        written += chunk;
    }

    CloseHandle(file);
    if (!ok) {
        error = L"Failed to write \"" + path + L"\".";
        return false;
    }
    return true;
}

std::wstring App::FormatBytes(size_t bytes) {
    return std::to_wstring(bytes) + L" bytes";
}

bool App::Initialize(int showCommand) {
    HDC screen = GetDC(nullptr);
    dpi_ = GetDeviceCaps(screen, LOGPIXELSY);
    ReleaseDC(nullptr, screen);
    if (dpi_ <= 0) dpi_ = 96;

    headerFont_ = MakeFont(15, true);
    bodyFont_ = MakeFont(10, false);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProcStatic;
    wc.hInstance = instance_;
    wc.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_BOOTSTRAPPER));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kWindowClass;
    wc.hIconSm = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_SMALL));

    if (!RegisterClassExW(&wc)) return false;

    hwnd_ = CreateWindowExW(0, kWindowClass, kWindowTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, Scale(460), Scale(290), nullptr, nullptr, instance_, this);
    if (!hwnd_) return false;

    CreateControls();
    ApplyTheme();
    Layout();

    ShowWindow(hwnd_, showCommand);
    UpdateWindow(hwnd_);
    return true;
}

int App::Run() {
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd_, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return static_cast<int>(message.wParam);
}

void App::CreateControls() {
    auto makeStatic = [&](const wchar_t* text, DWORD style, int id) {
        return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
    };
    auto makeButton = [&](const wchar_t* text, int id) {
        return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
    };

    header_ = makeStatic(L"NRMM Bootstrapper", SS_LEFT | SS_NOPREFIX, IDC_HEADER);
    status_ = makeStatic(L"Ready.", SS_LEFT | SS_OWNERDRAW, IDC_STATUS);

    btnInject_ = makeButton(L"Load Mod Menu", IDC_BTN_INJECT);
    btnDecrypt_ = makeButton(L"Decrypt Game Save", IDC_BTN_DECRYPT);
    btnEncrypt_ = makeButton(L"Encrypt Game Save", IDC_BTN_ENCRYPT);

    SendMessageW(header_, WM_SETFONT, reinterpret_cast<WPARAM>(headerFont_), TRUE);

    HWND buttons[] = {btnInject_, btnDecrypt_, btnEncrypt_};
    for (HWND button : buttons) {
        SetWindowSubclass(button, ButtonProcStatic, 1, reinterpret_cast<DWORD_PTR>(this));
    }
}

void App::Layout() {
    const int margin = Scale(24);
    const int clientWidth = Scale(460);
    const int contentWidth = clientWidth - margin * 2;

    auto place = [](HWND control, int x, int y, int width, int height) {
        if (control) SetWindowPos(control, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    };

    place(header_, margin, Scale(20), contentWidth, Scale(32));
    place(btnInject_, margin, Scale(72), contentWidth, Scale(46));

    const int gap = Scale(12);
    const int half = (contentWidth - gap) / 2;
    place(btnDecrypt_, margin, Scale(130), half, Scale(42));
    place(btnEncrypt_, margin + half + gap, Scale(130), contentWidth - half - gap, Scale(42));

    const int statusHeight = Scale(46);
    place(status_, margin, Scale(186), contentWidth, statusHeight);

    const int clientHeight = Scale(186) + statusHeight + Scale(24);

    RECT rect = {0, 0, clientWidth, clientHeight};
    AdjustWindowRectEx(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0);
    SetWindowPos(hwnd_, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void App::ApplyTheme() {
    theme_ = SystemUsesLightTheme() ? &LightTheme() : &DarkTheme();

    if (backgroundBrush_) DeleteObject(backgroundBrush_);
    backgroundBrush_ = CreateSolidBrush(theme_->windowBg);

    ApplyTitleBarTheme();

    InvalidateRect(hwnd_, nullptr, TRUE);
    HWND controls[] = {header_, status_, btnInject_, btnDecrypt_, btnEncrypt_};
    for (HWND control : controls) {
        if (control) InvalidateRect(control, nullptr, TRUE);
    }
}

void App::ApplyTitleBarTheme() {
    const BOOL dark = theme_->dark ? TRUE : FALSE;
    // 20 = DWMWA_USE_IMMERSIVE_DARK_MODE, 19 = pre-20H1 fallback.
    if (FAILED(DwmSetWindowAttribute(hwnd_, 20, &dark, sizeof(dark)))) DwmSetWindowAttribute(hwnd_, 19, &dark, sizeof(dark));
}

void App::DrawButton(const DRAWITEMSTRUCT* item) {
    const Theme& theme = *theme_;
    const bool selected = (item->itemState & ODS_SELECTED) != 0;
    const bool disabled = (item->itemState & ODS_DISABLED) != 0;
    const bool hot = item->CtlID == static_cast<UINT>(hotId_);

    COLORREF fill = theme.button;
    if (disabled) fill = theme.panelBg;
    else if (selected) fill = theme.buttonActive;
    else if (hot) fill = theme.buttonHover;

    RECT rect = item->rcItem;

    HBRUSH fillBrush = CreateSolidBrush(fill);
    FillRect(item->hDC, &rect, fillBrush);
    DeleteObject(fillBrush);

    HBRUSH borderBrush = CreateSolidBrush(theme.border);
    FrameRect(item->hDC, &rect, borderBrush);
    DeleteObject(borderBrush);

    wchar_t text[256];
    GetWindowTextW(item->hwndItem, text, 256);

    RECT textRect = rect;
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, disabled ? theme.muted : theme.text);
    HFONT previous = static_cast<HFONT>(SelectObject(item->hDC, bodyFont_));
    DrawTextW(item->hDC, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(item->hDC, previous);
}

void App::DrawStatus(const DRAWITEMSTRUCT* item) {
    const Theme& theme = *theme_;
    RECT rect = item->rcItem;

    HBRUSH fillBrush = CreateSolidBrush(theme.panelBg);
    FillRect(item->hDC, &rect, fillBrush);
    DeleteObject(fillBrush);

    HBRUSH borderBrush = CreateSolidBrush(theme.border);
    FrameRect(item->hDC, &rect, borderBrush);
    DeleteObject(borderBrush);

    RECT stripe = rect;
    stripe.right = stripe.left + Scale(3);
    HBRUSH stripeBrush = CreateSolidBrush(statusError_ ? theme.error : theme.accent);
    FillRect(item->hDC, &stripe, stripeBrush);
    DeleteObject(stripeBrush);

    wchar_t text[1024];
    GetWindowTextW(item->hwndItem, text, 1024);

    RECT textRect = rect;
    textRect.left += Scale(16);
    textRect.right -= Scale(12);
    textRect.top += Scale(7);
    textRect.bottom -= Scale(7);

    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, statusError_ ? theme.error : theme.text);
    HFONT previous = static_cast<HFONT>(SelectObject(item->hDC, bodyFont_));
    DrawTextW(item->hDC, text, -1, &textRect, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(item->hDC, previous);
}

void App::OnCommand(int id) {
    switch (id) {
        case IDC_BTN_INJECT: OnInject(); break;
        case IDC_BTN_DECRYPT: OnDecrypt(); break;
        case IDC_BTN_ENCRYPT: OnEncrypt(); break;
        default: break;
    }
}

void App::OnInject() {
    const DWORD processId = Injector::FindProcessId(kGameProcess);
    if (processId == 0) {
        SetStatus(L"The game is not running (NIGHT-RUNNERS PRIVATE ALPHA). Launch it first.", true);
        return;
    }

    const std::wstring directory = ExecutableDirectory();
    if (directory.empty()) {
        SetStatus(L"Could not determine the bootstrapper's folder.", true);
        return;
    }

    const std::wstring debugPath = directory + L"\\" + kDebugDll;
    const std::wstring releasePath = directory + L"\\" + kReleaseDll;

    std::wstring dllPath;
    std::wstring moduleName;
    if (FileExists(debugPath)) {
        dllPath = debugPath;
        moduleName = kDebugDll;
    } else if (FileExists(releasePath)) {
        dllPath = releasePath;
        moduleName = kReleaseDll;
    } else {
        SetStatus(L"nrmm.dll was not found next to the bootstrapper.", true);
        return;
    }

    if (Injector::IsModuleLoaded(processId, moduleName)) {
        SetStatus(moduleName + L" is already loaded in the game.", false);
        return;
    }

    std::wstring error;
    if (!Injector::Inject(processId, dllPath, error)) {
        SetStatus(error, true);
        return;
    }

    SetStatus(L"Injected " + moduleName + L" into the game (PID " + std::to_wstring(processId) + L").", false);
}

void App::OnDecrypt() {
    const std::wstring outputDirectory = ExecutableDirectory();
    if (outputDirectory.empty()) {
        SetStatus(L"Could not determine the bootstrapper's folder.", true);
        return;
    }

    const std::wstring saveDirectory = GameSaveDirectory();
    if (saveDirectory.empty()) {
        SetStatus(L"Could not locate the game's save folder.", true);
        return;
    }

    const std::wstring inputPath = saveDirectory + L"\\SaveFile.es3";
    const std::wstring outputPath = outputDirectory + L"\\SaveFile.json";
    if (!FileExists(inputPath)) {
        SetStatus(L"SaveFile.es3 was not found. Launch the game once so it creates a save.", true);
        return;
    }

    std::vector<uint8_t> data;
    std::vector<uint8_t> plaintext;
    std::wstring error;
    if (!ReadFileBytes(inputPath, data, error)) {
        SetStatus(error, true);
        return;
    }

    bool wasGzipped = false;
    if (!SaveCrypto::Decrypt(data, kSavePassword, plaintext, wasGzipped, error)) {
        SetStatus(error, true);
        return;
    }

    if (!WriteFileBytes(outputPath, plaintext, error)) {
        SetStatus(error, true);
        return;
    }

    std::wstring message = L"Decrypted SaveFile.es3 -> SaveFile.json next to the bootstrapper (" + FormatBytes(plaintext.size());
    message += wasGzipped ? L", gzip decompressed)." : L").";
    SetStatus(message, false);
}

void App::OnEncrypt() {
    const std::wstring inputDirectory = ExecutableDirectory();
    if (inputDirectory.empty()) {
        SetStatus(L"Could not determine the bootstrapper's folder.", true);
        return;
    }

    const std::wstring saveDirectory = GameSaveDirectory();
    if (saveDirectory.empty()) {
        SetStatus(L"Could not locate the game's save folder.", true);
        return;
    }

    const std::wstring inputPath = inputDirectory + L"\\SaveFile.json";
    const std::wstring outputPath = saveDirectory + L"\\SaveFile.es3";
    if (!FileExists(inputPath)) {
        SetStatus(L"SaveFile.json was not found next to the bootstrapper. Decrypt a save first.", true);
        return;
    }

    std::vector<uint8_t> data;
    std::vector<uint8_t> ciphertext;
    std::wstring error;
    if (!ReadFileBytes(inputPath, data, error)) {
        SetStatus(error, true);
        return;
    }

    if (!SaveCrypto::Encrypt(data, kSavePassword, false, ciphertext, error)) {
        SetStatus(error, true);
        return;
    }

    if (!WriteFileBytes(outputPath, ciphertext, error)) {
        SetStatus(error, true);
        return;
    }

    SetStatus(L"Encrypted SaveFile.json -> SaveFile.es3 in the game's save folder (" + FormatBytes(ciphertext.size()) + L").", false);
}

void App::SetHot(int controlId, bool hot) {
    if (hot) {
        if (hotId_ == controlId) return;
        hotId_ = controlId;
    } else {
        if (hotId_ != controlId) return;
        hotId_ = 0;
    }

    HWND controls[] = {btnInject_, btnDecrypt_, btnEncrypt_};
    for (HWND control : controls) {
        if (control && GetDlgCtrlID(control) == controlId) {
            InvalidateRect(control, nullptr, TRUE);
            return;
        }
    }
}

void App::SetStatus(const std::wstring& text, bool error) {
    statusError_ = error;
    SetWindowTextW(status_, text.c_str());
    InvalidateRect(status_, nullptr, TRUE);
}

int App::Scale(int value) const {
    return MulDiv(value, dpi_, 96);
}

HFONT App::MakeFont(int pointSize, bool bold) const {
    return CreateFontW(-MulDiv(pointSize, dpi_, 72), 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

LRESULT CALLBACK App::WindowProcStatic(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    App* self = nullptr;
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<App*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) return self->WindowProc(hwnd, message, wParam, lParam);
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK App::ButtonProcStatic(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData) {
    auto* self = reinterpret_cast<App*>(refData);

    switch (message) {
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT track{};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = hwnd;
            TrackMouseEvent(&track);
            self->SetHot(GetDlgCtrlID(hwnd), true);
            break;
        }
        case WM_MOUSELEAVE:
            self->SetHot(GetDlgCtrlID(hwnd), false);
            break;
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, ButtonProcStatic, subclassId);
            break;
        default:
            break;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

LRESULT App::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_COMMAND:
            OnCommand(LOWORD(wParam));
            return 0;

        case WM_DRAWITEM: {
            auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (item->CtlID == IDC_STATUS) DrawStatus(item);
            else DrawButton(item);
            return TRUE;
        }

        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, theme_->text);
            SetBkColor(dc, theme_->windowBg);
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(backgroundBrush_);
        }

        case WM_ERASEBKGND: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect(dc, &rect, backgroundBrush_);
            return 1;
        }

        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(hwnd, &paint);
            const int x = Scale(24);
            const int y = Scale(56);
            RECT bar = {x, y, x + Scale(120), y + Scale(3)};
            HBRUSH accent = CreateSolidBrush(theme_->accent);
            FillRect(dc, &bar, accent);
            DeleteObject(accent);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_SETTINGCHANGE:
            if (wParam == 0 && lParam && _wcsicmp(reinterpret_cast<const wchar_t*>(lParam), L"ImmersiveColorSet") == 0) ApplyTheme();
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY: {
            if (backgroundBrush_) {
                DeleteObject(backgroundBrush_);
                backgroundBrush_ = nullptr;
            }
            if (headerFont_) DeleteObject(headerFont_);
            if (bodyFont_) DeleteObject(bodyFont_);
            PostQuitMessage(0);
            return 0;
        }

        default:
            break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}
