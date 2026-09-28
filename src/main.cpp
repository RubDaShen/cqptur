#include "app.h"
#include "error.h"
#include "startup.h"

#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <cwchar>
#include <exception>

namespace {

//  "Local\" makes it per sign-in session, so each signed-in user gets their own cqptur.
constexpr wchar_t kInstanceMutex[] = L"Local\\cqptur.running";

bool HasArgument(const wchar_t* argument) {
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    bool found = false;

    for (int i = 1; (i < count) && !found; ++i) {
        found = (std::wcscmp(arguments[i], argument) == 0);
    }
    LocalFree(arguments);

    return found;
}

void RequestCaptureFromRunningCopy() {
    const HWND running = FindWindowW(kMainWindowClass, nullptr);
    if (!running) {
        return; // it's still starting up
    }

    //  The user just started us, so Windows lets us pass on the right to come to the
    //  foreground. The running copy needs it for its overlay to get the keyboard (Esc).
    DWORD processId = 0;
    GetWindowThreadProcessId(running, &processId);
    AllowSetForegroundWindow(processId);

    PostMessageW(running, kCaptureRequestMessage, 0, 0);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    //  Everything works in physical pixels, which relies on app.manifest making the process
    //  per-monitor DPI aware. Fail loudly if a build ever loses it, rather than capture scaled.
    if (
        !AreDpiAwarenessContextsEqual(
            GetThreadDpiAwarenessContext(),
            DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
        )
    ) {
        ShowError("This build is missing its DPI-awareness manifest, so screenshots would be scaled.");
        return 1;
    }

    const bool quietStart = HasArgument(kStartupFlag);

    //  Only one cqptur runs at a time. Starting it again (e.g. from the Start menu) asks the
    //  running copy for a screenshot instead.
    const HANDLE instanceMutex = CreateMutexW(nullptr, FALSE, kInstanceMutex);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (!quietStart) {
            RequestCaptureFromRunningCopy();
        }
        CloseHandle(instanceMutex);

        return 0;
    }

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        ShowError("Could not initialize COM.");
        return 1;
    }

    int exitCode = 0;
    try {
        exitCode = RunTrayApp(instance, quietStart);
    }
    catch (const std::exception& e) {
        ShowError(e.what());
        exitCode = 1;
    }

    CoUninitialize();
    CloseHandle(instanceMutex);

    return exitCode;
}
