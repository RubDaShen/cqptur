#pragma once

#include <windows.h>

#include <format>
#include <stdexcept>

//  Failures are thrown as exceptions, caught where a user action starts (startup, a capture,
//  a menu choice) and shown with ShowError.

inline void ShowError(const char* message) {
    MessageBoxA(nullptr, message, "cqptur", MB_ICONERROR | MB_OK | MB_SETFOREGROUND);
}

//  `error` defaults to GetLastError() at the call site; pass it explicitly when cleanup
//  operations may have overwritten it in between.
[[noreturn]] inline void ThrowWin32Error(const char* what, DWORD error = GetLastError()) {
    throw std::runtime_error(std::format("{} failed (Windows error {}).", what, error));
}

inline void ThrowIfFailed(HRESULT hr, const char* what) {
    if (FAILED(hr)) {
        throw std::runtime_error(
            std::format("{} failed (HRESULT 0x{:08X}).", what, static_cast<unsigned long>(hr)));
    }
}
