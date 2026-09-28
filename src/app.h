#pragma once

#include <windows.h>

//  Class of the hidden main window. A second copy of cqptur uses it to find the first one.
inline constexpr wchar_t kMainWindowClass[] = L"CqpturMain";

//  Asks the running copy of cqptur to take a screenshot (posted by a second copy).
inline constexpr UINT kCaptureRequestMessage = WM_APP + 1;

//  Runs cqptur from the notification area until the user picks Exit, and returns the exit
//  code. `quietStart` skips the "cqptur is running" notification, for when Windows starts
//  cqptur at sign-in.
int RunTrayApp(HINSTANCE instance, bool quietStart);
