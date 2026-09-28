#include "tray.h"

#include <cwchar>

namespace {

constexpr UINT kIconId = 1;

} // namespace

TrayIcon::TrayIcon(HWND owner, UINT callbackMessage, HICON icon, const wchar_t* tooltip)
    : data_{} {
    data_.cbSize = sizeof(data_);
    data_.hWnd = owner;
    data_.uID = kIconId;
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data_.uCallbackMessage = callbackMessage;
    data_.hIcon = icon;
    data_.uVersion = NOTIFYICON_VERSION_4;
    wcsncpy_s(data_.szTip, tooltip, _TRUNCATE);
}

TrayIcon::~TrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &data_);
}

bool TrayIcon::Show() {
    if (!Shell_NotifyIconW(NIM_ADD, &data_)) {
        return false;
    }

    return Shell_NotifyIconW(NIM_SETVERSION, &data_) != FALSE;
}

void TrayIcon::Notify(const std::wstring& title, const std::wstring& text) {
    NOTIFYICONDATAW info = data_;
    info.uFlags = NIF_INFO;
    info.dwInfoFlags = NIIF_NONE | NIIF_NOSOUND | NIIF_RESPECT_QUIET_TIME;
    wcsncpy_s(info.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(info.szInfo, text.c_str(), _TRUNCATE);

    Shell_NotifyIconW(NIM_MODIFY, &info);
}
