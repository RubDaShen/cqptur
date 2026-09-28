#include "app.h"

#include "capture.h"
#include "error.h"
#include "output.h"
#include "overlay.h"
#include "resource.h"
#include "settings.h"
#include "startup.h"
#include "tray.h"

#include <shellapi.h>
#include <windowsx.h>

#include <exception>
#include <filesystem>
#include <format>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kTooltip[] = L"cqptur (click to capture)";
constexpr UINT kTrayMessage = WM_APP + 2;
constexpr int kHotkeyId = 1;
constexpr UINT_PTR kCaptureTimerId = 1;

//  The capture shortcuts to choose from; the first is the default. Plain Print Screen isn't
//  one of them: Windows keeps it for itself (Snipping Tool, or its classic copy to the
//  clipboard) and only yields it to an app whose window is in the foreground, which a tray
//  app never is. With a modifier it's an ordinary hotkey.
struct Shortcut {
    UINT modifiers;
    const wchar_t* name;
};

constexpr Shortcut kShortcuts[] = {
    {MOD_CONTROL, L"Ctrl+Print Screen"},
    {MOD_SHIFT, L"Shift+Print Screen"},
    {MOD_CONTROL | MOD_SHIFT, L"Ctrl+Shift+Print Screen"},
};
constexpr UINT kShortcutKey = VK_SNAPSHOT;

//  The setting that remembers the chosen shortcut's modifiers.
constexpr wchar_t kShortcutSetting[] = L"Shortcut";

//  A short pause before capturing from the tray icon or its menu, so the closing menu or
//  tray flyout doesn't end up in the screenshot.
constexpr UINT kCaptureDelayMs = 200;

//  Tray menu commands.
enum Command : UINT {
    kCommandCapture = 1,
    kCommandStartWithWindows,
    kCommandExit,
    kCommandShortcut = 100, // the first of one command per entry in kShortcuts, in order
};

//  What clicking the latest notification does.
enum class NotificationAction {
    None,
    OpenScreenshot,
};

struct SavedScreenshot {
    std::filesystem::path path;
    int width;
    int height;
};

//  One complete capture: freeze the screen, let the user choose an area, then copy and save
//  it. Returns nothing if the user cancelled.
std::optional<SavedScreenshot> TakeScreenshot(HINSTANCE instance) {
    const Screenshot shot = CaptureVirtualScreen();
    const std::optional<RECT> area = SelectRegion(instance, shot);

    if (!area) {
        return std::nullopt;
    }

    const Image image = Crop(shot.image, *area);
    const std::vector<uint8_t> png = EncodePng(image);
    CopyToClipboard(image, png);

    return SavedScreenshot{SaveToScreenshotsFolder(png), image.width, image.height};
}

//  The saved shortcut's position in kShortcuts, or the default's if nothing valid was saved.
size_t LoadShortcut() {
    const std::optional<DWORD> saved = ReadSetting(kShortcutSetting);

    for (size_t i = 0; saved && (i < std::size(kShortcuts)); ++i) {
        if (kShortcuts[i].modifiers == *saved) {
            return i;
        }
    }

    return 0;
}

//  The hidden main window behind the tray icon: it owns the icon, the capture hotkey and the
//  tray menu, and runs each capture.
class App {
//	|-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-|
//				    Members and Fields
//	|-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-|

    //	-------------------------------------------
    //			        Variables
    //	-------------------------------------------

private:
    HINSTANCE instance_;
    HWND window_ = nullptr;
    HICON icon_ = nullptr;
    std::unique_ptr<TrayIcon> tray_;
    UINT taskbarCreatedMessage_; // broadcast when Explorer, and with it the tray, restarts

    size_t shortcut_ = 0; // position in kShortcuts
    bool hotkeyRegistered_ = false;
    bool capturing_ = false;
    NotificationAction notificationAction_ = NotificationAction::None;
    std::filesystem::path lastScreenshot_;

//	|-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-|
//			        Functions and Methods
//	|-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-|

    //	-------------------------------------------
    //		    Constructors and Destructors
    //	-------------------------------------------

public:
    explicit App(HINSTANCE instance);
    App(const App&) = delete;

    ~App();

    //	-------------------------------------------
    //			        Functions
    //	-------------------------------------------

public:
    App& operator=(const App&) = delete;

    int Run(bool quietStart);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void OnTrayEvent(UINT event, POINT anchor);
    void OnCommand(UINT command);
    void ShowMenu(POINT at);
    void RegisterCaptureHotkey();
    void ChangeShortcut(size_t index);

    void CaptureSoon();
    void Capture();

    void GreetUser();
    void Notify(const std::wstring& title, const std::wstring& text, NotificationAction action);
    void OnNotificationClicked();
};

App::App(HINSTANCE instance)
    : instance_(instance), taskbarCreatedMessage_(RegisterWindowMessageW(L"TaskbarCreated")) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kMainWindowClass;

    if (!RegisterClassExW(&wc)) {
        ThrowWin32Error("Registering the main window class");
    }

    //  Never shown: it only receives hotkey, tray and timer messages. It's a regular
    //  top-level window rather than a message-only one because only top-level windows hear
    //  about Explorer restarts.
    window_ = CreateWindowExW(
        0,
        kMainWindowClass,
        L"cqptur",
        WS_OVERLAPPED,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        instance,
        this
    );

    if (!window_) {
        ThrowWin32Error("Creating the main window");
    }

    //  Sized for the notification area at the current display scale.
    const UINT dpi = GetDpiForSystem();
    icon_ = static_cast<HICON>(
        LoadImageW(
            instance,
            MAKEINTRESOURCEW(IDI_APP),
            IMAGE_ICON,
            GetSystemMetricsForDpi(SM_CXSMICON, dpi),
            GetSystemMetricsForDpi(SM_CYSMICON, dpi),
            LR_DEFAULTCOLOR
        )
    );

    tray_ = std::make_unique<TrayIcon>(window_, kTrayMessage, icon_, kTooltip);
}

App::~App() {
    if (IsWindow(window_)) {
        DestroyWindow(window_);
    }
    if (icon_) {
        DestroyIcon(icon_);
    }
}

int App::Run(bool quietStart) {
    //  Fails if Explorer isn't up yet (early at sign-in); TaskbarCreated brings us back then.
    tray_->Show();
    shortcut_ = LoadShortcut();
    RegisterCaptureHotkey();

    if (!quietStart) {
        GreetUser();
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK App::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (!app) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return app->HandleMessage(hwnd, message, wParam, lParam);
}

LRESULT App::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if ((taskbarCreatedMessage_ != 0) && (message == taskbarCreatedMessage_)) {
        tray_->Show();
        return 0;
    }

    switch (message) {
    case WM_HOTKEY:
        if (wParam == kHotkeyId) {
            Capture();
        }
        return 0;

    case kCaptureRequestMessage:
        CaptureSoon(); // the Start menu or launcher that started the second copy may still be closing
        return 0;

    case kTrayMessage:
        OnTrayEvent(LOWORD(lParam), {GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam)});
        return 0;

    case WM_COMMAND:
        OnCommand(LOWORD(wParam));
        return 0;

    case WM_TIMER:
        if (wParam == kCaptureTimerId) {
            KillTimer(hwnd, kCaptureTimerId);
            Capture();
        }
        return 0;

    case WM_DESTROY:
        tray_.reset();
        UnregisterHotKey(hwnd, kHotkeyId);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void App::OnTrayEvent(UINT event, POINT anchor) {
    switch (event) {
    case NIN_SELECT:    // left click
    case NIN_KEYSELECT: // Enter or Space while the icon has keyboard focus
        CaptureSoon();
        break;

    case WM_CONTEXTMENU:
        ShowMenu(anchor);
        break;

    case NIN_BALLOONUSERCLICK:
        OnNotificationClicked();
        break;
    }
}

void App::OnCommand(UINT command) {
    if ((command >= kCommandShortcut) && (command < kCommandShortcut + std::size(kShortcuts))) {
        try {
            ChangeShortcut(command - kCommandShortcut);
        }
        catch (const std::exception& e) {
            ShowError(e.what());
        }

        return;
    }

    switch (command) {
    case kCommandCapture:
        CaptureSoon();
        break;

    case kCommandStartWithWindows:
        try {
            SetStartWithWindows(!IsStartWithWindowsEnabled());
        }
        catch (const std::exception& e) {
            ShowError(e.what());
        }
        break;

    case kCommandExit:
        DestroyWindow(window_);
        break;
    }
}

void App::ShowMenu(POINT at) {
    //  Another app may have let go of the shortcut since cqptur started.
    if (!hotkeyRegistered_) {
        RegisterCaptureHotkey();
    }

    //  The shortcut sits right-aligned next to the item, the way menus show them.
    const std::wstring captureItem =
        hotkeyRegistered_ ? std::format(L"Capture\t{}", kShortcuts[shortcut_].name) : L"Capture";

    //  One radio item per shortcut, the one in use marked.
    HMENU shortcuts = CreatePopupMenu();
    for (size_t i = 0; i < std::size(kShortcuts); ++i) {
        AppendMenuW(shortcuts, MF_STRING, kCommandShortcut + i, kShortcuts[i].name);
    }
    CheckMenuRadioItem(
        shortcuts,
        kCommandShortcut,
        static_cast<UINT>(kCommandShortcut + std::size(kShortcuts) - 1),
        static_cast<UINT>(kCommandShortcut + shortcut_),
        MF_BYCOMMAND
    );

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, kCommandCapture, captureItem.c_str());
    SetMenuDefaultItem(menu, kCommandCapture, FALSE);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(shortcuts), L"Shortcut");
    AppendMenuW(
        menu,
        MF_STRING | (IsStartWithWindowsEnabled() ? MF_CHECKED : MF_UNCHECKED),
        kCommandStartWithWindows,
        L"Start with Windows"
    );
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCommandExit, L"Exit");

    //  Without the foreground switch before and the WM_NULL after, the menu doesn't close
    //  when the user clicks somewhere else.
    SetForegroundWindow(window_);
    TrackPopupMenuEx(menu, TPM_RIGHTBUTTON, at.x, at.y, window_, nullptr);
    PostMessageW(window_, WM_NULL, 0, 0);

    DestroyMenu(menu); // takes the Shortcut submenu with it
}

void App::RegisterCaptureHotkey() {
    hotkeyRegistered_ = RegisterHotKey(
        window_,
        kHotkeyId,
        kShortcuts[shortcut_].modifiers | MOD_NOREPEAT,
        kShortcutKey
    ) != FALSE;
}

void App::ChangeShortcut(size_t index) {
    const size_t previous = shortcut_;

    UnregisterHotKey(window_, kHotkeyId);
    shortcut_ = index;
    RegisterCaptureHotkey();

    if (!hotkeyRegistered_) {
        //  Another app already uses it, so stay with the previous shortcut.
        shortcut_ = previous;
        RegisterCaptureHotkey();
        Notify(
            L"Shortcut not changed",
            std::format(L"{} is already used by another app.", kShortcuts[index].name),
            NotificationAction::None
        );

        return;
    }

    //  The default isn't stored, so a default setup leaves nothing in the registry.
    if (index == 0) {
        DeleteSetting(kShortcutSetting);
    }
    else {
        WriteSetting(kShortcutSetting, kShortcuts[index].modifiers);
    }
}

void App::CaptureSoon() {
    SetTimer(window_, kCaptureTimerId, kCaptureDelayMs, nullptr);
}

void App::Capture() {
    //  The overlay runs its own message loop, so another request can arrive mid-capture.
    if (capturing_) {
        return;
    }
    capturing_ = true;

    try {
        if (const std::optional<SavedScreenshot> saved = TakeScreenshot(instance_)) {
            lastScreenshot_ = saved->path;
            Notify(
                std::format(L"Screenshot copied ({} × {})", saved->width, saved->height),
                L"Also saved to your Screenshots folder. Click to open it.",
                NotificationAction::OpenScreenshot
            );
        }
    }
    catch (const std::exception& e) {
        ShowError(e.what());
    }

    capturing_ = false;
}

//  Says that cqptur is running (new tray icons start out hidden on Windows 11) and how to
//  use it.
void App::GreetUser() {
    if (!hotkeyRegistered_) {
        Notify(
            L"cqptur is running",
            std::format(
                L"Another app is using {}. Click the cqptur icon to take screenshots, or pick "
                L"another shortcut from its menu.",
                kShortcuts[shortcut_].name
            ),
            NotificationAction::None
        );
    }
    else {
        Notify(
            L"cqptur is running",
            std::format(L"Press {} to take a screenshot.", kShortcuts[shortcut_].name),
            NotificationAction::None
        );
    }
}

void App::Notify(const std::wstring& title, const std::wstring& text, NotificationAction action) {
    notificationAction_ = action;
    tray_->Notify(title, text);
}

void App::OnNotificationClicked() {
    switch (notificationAction_) {
    case NotificationAction::OpenScreenshot:
        ShellExecuteW(nullptr, L"open", lastScreenshot_.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break;

    case NotificationAction::None:
        break;
    }
}

} // namespace

int RunTrayApp(HINSTANCE instance, bool quietStart) {
    App app(instance);

    return app.Run(quietStart);
}
