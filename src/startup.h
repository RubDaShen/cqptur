#pragma once

//  "Start with Windows", done the standard per-user way: a value under
//  HKCU\Software\Microsoft\Windows\CurrentVersion\Run. It needs no admin rights and shows
//  up in Task Manager's Startup apps page like any other app.

//  Added to the command line when Windows starts cqptur at sign-in, so it starts quietly.
inline constexpr wchar_t kStartupFlag[] = L"--startup";

//  True only if Windows will start *this* copy of cqptur.exe at sign-in.
bool IsStartWithWindowsEnabled();

void SetStartWithWindows(bool enabled);
