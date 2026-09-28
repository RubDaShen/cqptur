#include "capture.h"

#include "error.h"

#include <utility>

DibSection::DibSection(int width, int height) : width_(width), height_(height) {
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;  // negative = top-down rows
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;

    bitmap_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap_) {
        ThrowWin32Error("Allocating a bitmap");
    }

    pixels_ = static_cast<uint32_t*>(bits);
}

DibSection::DibSection(DibSection&& other) noexcept
    : width_(other.width_),
      height_(other.height_),
      bitmap_(std::exchange(other.bitmap_, nullptr)),
      pixels_(std::exchange(other.pixels_, nullptr)) {}

DibSection::~DibSection() {
    if (bitmap_) {
        DeleteObject(bitmap_);
    }
}

Screenshot CaptureVirtualScreen() {
    //  The manifest makes this process per-monitor DPI aware, so these are physical pixels
    //  spanning all monitors, not the scaled-down numbers a DPI-unaware app would get.
    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    Screenshot shot{{x, y}, DibSection(width, height)};

    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);

    const HGDIOBJ previous = SelectObject(memory, shot.image.Handle());
    const BOOL copied = BitBlt(memory, 0, 0, width, height, screen, x, y, SRCCOPY | CAPTUREBLT); // CAPTUREBLT also grabs layered windows (tooltips, translucent menus).
    const DWORD error = GetLastError();

    SelectObject(memory, previous);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);

    if (!copied) {
        ThrowWin32Error("Capturing the screen", error);
    }

    //  GDI must finish writing before we read the pixels directly.
    GdiFlush(); 
    return shot;
}
