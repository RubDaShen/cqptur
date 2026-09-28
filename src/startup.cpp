#include "startup.h"

#include "error.h"

#include <windows.h>

#include <cwchar>
#include <format>
#include <string>

namespace {

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"cqptur";

//  Where Task Manager remembers that the user disabled a startup app. It leaves the Run
//  value in place, so this has to be checked as well.
constexpr wchar_t kApprovedKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";

std::wstring ExePath() {
    std::wstring path(MAX_PATH, L'\0');

    for (;;) {
        const DWORD length = GetModuleFileNameW(
            nullptr,
            path.data(),
            static_cast<DWORD>(path.size())
        );

        if (length == 0) {
            ThrowWin32Error("Finding cqptur.exe");
        }
        if (length < path.size()) {
            path.resize(length);

            return path;
        }

        path.resize(path.size() * 2); // the path was cut off; try a bigger buffer
    }
}

//  What the Run value holds when it starts this copy of cqptur.
std::wstring StartupCommand() {
    return std::format(L"\"{}\" {}", ExePath(), kStartupFlag);
}

std::wstring ReadRunValue() {
    DWORD size = 0;
    if (
        RegGetValueW(
            HKEY_CURRENT_USER, kRunKey, kValueName, RRF_RT_REG_SZ, nullptr, nullptr, &size
        ) != ERROR_SUCCESS
    ) {
        return {};
    }

    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (
        RegGetValueW(
            HKEY_CURRENT_USER, kRunKey, kValueName, RRF_RT_REG_SZ, nullptr, value.data(), &size
        ) != ERROR_SUCCESS
    ) {
        return {};
    }

    value.resize(std::wcslen(value.c_str())); // drop the terminating null

    return value;
}

bool DisabledInTaskManager() {
    BYTE data[64]{};
    DWORD size = sizeof(data);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        kApprovedKey,
        kValueName,
        RRF_RT_REG_BINARY,
        nullptr,
        data,
        &size
    );

    //  The first byte is even (2) while enabled and odd (3) once the user disables it.
    return (status == ERROR_SUCCESS) && (size > 0) && ((data[0] & 1) != 0);
}

} // namespace

bool IsStartWithWindowsEnabled() {
    const std::wstring command = ReadRunValue();

    return (
        !command.empty() &&
        (CompareStringOrdinal(command.c_str(), -1, StartupCommand().c_str(), -1, TRUE) == CSTR_EQUAL) &&
        !DisabledInTaskManager()
    );
}

void SetStartWithWindows(bool enabled) {
    //  Forget any "disabled" mark left by Task Manager, so the choice made here is what counts.
    RegDeleteKeyValueW(HKEY_CURRENT_USER, kApprovedKey, kValueName);

    LSTATUS status = ERROR_SUCCESS;
    if (enabled) {
        const std::wstring command = StartupCommand();
        status = RegSetKeyValueW(
            HKEY_CURRENT_USER,
            kRunKey,
            kValueName,
            REG_SZ,
            command.c_str(),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))
        );
    }
    else {
        status = RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kValueName);
        if (status == ERROR_FILE_NOT_FOUND) {
            status = ERROR_SUCCESS; // already off
        }
    }

    if (status != ERROR_SUCCESS) {
        ThrowWin32Error(
            enabled ? "Turning on Start with Windows" : "Turning off Start with Windows",
            static_cast<DWORD>(status)
        );
    }
}
