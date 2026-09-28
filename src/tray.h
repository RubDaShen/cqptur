#pragma once

#include <windows.h>
#include <shellapi.h>

#include <string>

//  cqptur's icon in the notification area (system tray), and the notifications it shows.
class TrayIcon {
//	|-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-|
//				    Members and Fields
//	|-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-|

    //	-------------------------------------------
    //			        Variables
    //	-------------------------------------------

private:
    NOTIFYICONDATAW data_;

//	|-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-|
//			        Functions and Methods
//	|-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-|

    //	-------------------------------------------
    //		    Constructors and Destructors
    //	-------------------------------------------

public:
    //  Clicks and notification events reach `owner` as `callbackMessage`, the
    //  NOTIFYICON_VERSION_4 way: the event is LOWORD(lParam), the click position is in wParam.
    TrayIcon(HWND owner, UINT callbackMessage, HICON icon, const wchar_t* tooltip);
    TrayIcon(const TrayIcon&) = delete;

    ~TrayIcon();

    //	-------------------------------------------
    //			        Functions
    //	-------------------------------------------

public:
    TrayIcon& operator=(const TrayIcon&) = delete;

    //  Adds the icon to the notification area. Explorer forgets it when it restarts, so this
    //  gets called again then.
    bool Show();

    //  A notification coming from the icon (shown as a toast on Windows 10 and 11).
    void Notify(const std::wstring& title, const std::wstring& text);
};
