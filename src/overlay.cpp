#include "overlay.h"

#include "error.h"

#include <shellscalingapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdlib>
#include <format>
#include <map>
#include <string>

namespace {

constexpr wchar_t kWindowClass[] = L"CqpturOverlay";
constexpr int kDimLevel = 140;  // brightness outside the selection, out of 256
constexpr COLORREF kAccent = RGB(0, 120, 215);
constexpr COLORREF kBoxBackground = RGB(32, 32, 32);
constexpr COLORREF kBoxText = RGB(255, 255, 255);
constexpr int kFontPoints = 10;

//  A memory DC with a bitmap selected into it, ready to be used as a BitBlt source.
class BitmapDC {
//	|-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-|
//				    Members and Fields
//	|-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-|

    //	-------------------------------------------
    //			        Variables
    //	-------------------------------------------

private:
    HDC dc_;
    HGDIOBJ previous_;

//	|-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-|
//			        Functions and Methods
//	|-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-|

    //	-------------------------------------------
    //			        Constructors
    //	-------------------------------------------

public:
    explicit BitmapDC(HBITMAP bitmap)
        : dc_(CreateCompatibleDC(nullptr)), previous_(SelectObject(dc_, bitmap)) {}
    
    BitmapDC(const BitmapDC&) = delete;

    ~BitmapDC() {
        SelectObject(dc_, previous_);
        DeleteDC(dc_);
    }

//	-------------------------------------------
//			        Functions
//	-------------------------------------------    
public:
    BitmapDC& operator=(const BitmapDC&) = delete;

    HDC Get() const { return dc_; }
};

DibSection MakeDimmedCopy(const DibSection& source) {
    DibSection dimmed(source.Width(), source.Height());
    const auto* in = reinterpret_cast<const uint8_t*>(source.Pixels());
    auto* out = reinterpret_cast<uint8_t*>(dimmed.Pixels());
    const size_t bytes = size_t(source.Width()) * source.Height() * 4;
    for (size_t i = 0; i < bytes; ++i) out[i] = static_cast<uint8_t>(in[i] * kDimLevel / 256);
    return dimmed;
}

struct TextBox {
    std::wstring text;
    HFONT font;
    RECT rect;
};

struct Overlay {
    explicit Overlay(const Screenshot& shot)
        : shot(shot),
          dimmed(MakeDimmedCopy(shot.image)),
          shotDC(shot.image.Handle()),
          dimmedDC(dimmed.Handle()) {}
    ~Overlay() {
        for (const auto& [dpi, font] : fonts) DeleteObject(font);
    }

    const Screenshot& shot;
    DibSection dimmed;
    BitmapDC shotDC;
    BitmapDC dimmedDC;
    std::map<UINT, HFONT> fonts;  // UI font per DPI, so text is the right size on every monitor

    POINT hintPoint{};     // the usage hint goes on the monitor containing this point
    bool showHint = true;  // until the first drag
    bool pressed = false;  // left button is down...
    bool dragging = false; // ...and has moved far enough to count as a drag
    POINT anchor{};        // where the button went down
    RECT selection{};      // empty until the user drags

    bool done = false;
    std::optional<RECT> result;
};

//  The overlay's client coordinates are screenshot pixel coordinates. These helpers take
//  such a point and look up the monitor under it.

HMONITOR MonitorAt(const Overlay& ov, POINT client) {
    const POINT desktop{client.x + ov.shot.origin.x, client.y + ov.shot.origin.y};

    return MonitorFromPoint(desktop, MONITOR_DEFAULTTONEAREST);
}

RECT MonitorRectAt(const Overlay& ov, POINT client) {
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(MonitorAt(ov, client), &info);
    RECT rect = info.rcMonitor;
    OffsetRect(&rect, -ov.shot.origin.x, -ov.shot.origin.y);

    return rect;
}

UINT DpiAt(const Overlay& ov, POINT client) {
    UINT dpiX = USER_DEFAULT_SCREEN_DPI;
    UINT dpiY = USER_DEFAULT_SCREEN_DPI;
    GetDpiForMonitor(MonitorAt(ov, client), MDT_EFFECTIVE_DPI, &dpiX, &dpiY);

    return dpiX;
}

int Scale(int value, UINT dpi) {
    return MulDiv(value, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
}

int BorderWidth(UINT dpi) {
    return Scale(2, dpi);
}

HFONT FontFor(Overlay& ov, UINT dpi) {
    HFONT& font = ov.fonts[dpi];
    if (!font) {
        font = CreateFontW(
            -MulDiv(
                kFontPoints,
                static_cast<int>(dpi),
                72
            ),
            0,
            0,
            0,
            FW_SEMIBOLD,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI"
        );
    }
    return font;
}

//  A padded box around `text` sized for `dpi`, with its top-left corner at (0, 0).
TextBox MakeTextBox(HDC dc, Overlay& ov, UINT dpi, std::wstring text) {
    const HFONT font = FontFor(ov, dpi);
    const HGDIOBJ previous = SelectObject(dc, font);
    SIZE size{};

    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    SelectObject(dc, previous);

    const int padX = Scale(8, dpi);
    const int padY = Scale(4, dpi);

    return {std::move(text), font, {0, 0, size.cx + 2 * padX, size.cy + 2 * padY}};
}

void DrawTextBox(HDC dc, const TextBox& box) {
    const HBRUSH background = CreateSolidBrush(kBoxBackground);
    FillRect(dc, &box.rect, background);
    DeleteObject(background);

    const HGDIOBJ previous = SelectObject(dc, box.font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, kBoxText);
    RECT rect = box.rect;
    DrawTextW(
        dc,
        box.text.c_str(),
        static_cast<int>(box.text.size()),
        &rect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX
    );

    SelectObject(dc, previous);
}

// Usage instructions, centered near the top of the monitor the cursor started on.
TextBox Hint(HDC dc, Overlay& ov) {
    const UINT dpi = DpiAt(ov, ov.hintPoint);
    TextBox box = MakeTextBox(
        dc, ov, dpi,
        L"Drag to capture an area   ·   Click to capture a whole screen   ·   Esc to cancel");
    const RECT monitor = MonitorRectAt(ov, ov.hintPoint);

    OffsetRect(
        &box.rect,
        (monitor.left + monitor.right - box.rect.right) / 2,
        monitor.top + Scale(24, dpi)
    );

    return box;
}

//  The "width × height" readout: just above the selection, or inside it when the
//  selection touches the top of its monitor.
TextBox SizeLabel(HDC dc, Overlay& ov, const RECT& selection) {
    const POINT corner{selection.left, selection.top};
    const UINT dpi = DpiAt(ov, corner);
    TextBox box = MakeTextBox(
        dc,
        ov,
        dpi,
        std::format(
            L"{} × {}", selection.right - selection.left,
            selection.bottom - selection.top
        )
    );
    const RECT monitor = MonitorRectAt(ov, corner);
    const int gap = BorderWidth(dpi) + Scale(4, dpi);
    const int width = box.rect.right;
    const int height = box.rect.bottom;
    int top = selection.top - gap - height;

    if (top < monitor.top) {
        top = selection.top + gap;
    }

    const int left = std::max(monitor.left, std::min(selection.left, monitor.right - width));
    OffsetRect(&box.rect, left, top);
    return box;
}

//  A frame just outside `rect`, so it never hides pixels that end up in the screenshot.
void DrawBorder(HDC dc, const RECT& rect, int width) {
    const HBRUSH brush = CreateSolidBrush(kAccent);
    const RECT sides[] = {
        {rect.left - width, rect.top - width, rect.right + width, rect.top},
        {rect.left - width, rect.bottom, rect.right + width, rect.bottom + width},
        {rect.left - width, rect.top, rect.left, rect.bottom},
        {rect.right, rect.top, rect.right + width, rect.bottom},
    };
    for (const RECT& side : sides) FillRect(dc, &side, brush);
    DeleteObject(brush);
}

//  Marks everything painted for `selection` (bright area, border, size label) for repaint.
void InvalidateSelection(HWND hwnd, Overlay& ov, const RECT& selection) {
    if (IsRectEmpty(&selection)) {
        return;
    }

    RECT area = selection;
    const int border = BorderWidth(DpiAt(ov, {selection.left, selection.top}));
    InflateRect(&area, border, border);
    InvalidateRect(hwnd, &area, FALSE);

    HDC dc = GetDC(hwnd);
    const RECT label = SizeLabel(dc, ov, selection).rect;
    ReleaseDC(hwnd, dc);
    InvalidateRect(hwnd, &label, FALSE);
}

void SetSelection(HWND hwnd, Overlay& ov, const RECT& selection) {
    InvalidateSelection(hwnd, ov, ov.selection);
    ov.selection = selection;
    InvalidateSelection(hwnd, ov, ov.selection);
}

void HideHint(HWND hwnd, Overlay& ov) {
    if (!ov.showHint) {
        return;
    }

    HDC dc = GetDC(hwnd);
    const RECT hint = Hint(dc, ov).rect;
    ReleaseDC(hwnd, dc);
    ov.showHint = false;
    InvalidateRect(hwnd, &hint, FALSE);
}

void Paint(HWND hwnd, Overlay& ov) {
    PAINTSTRUCT ps;
    HDC target = BeginPaint(hwnd, &ps);
    const RECT& dirty = ps.rcPaint;
    const int width = dirty.right - dirty.left;
    const int height = dirty.bottom - dirty.top;

    if (width > 0 && height > 0) {
        //  Compose the dirty area off-screen first so dragging doesn't flicker.
        HDC dc = CreateCompatibleDC(target);
        const HBITMAP buffer = CreateCompatibleBitmap(target, width, height);
        const HGDIOBJ previous = SelectObject(dc, buffer);
        SetViewportOrgEx(dc, -dirty.left, -dirty.top, nullptr); // keep drawing in client coords

        BitBlt(
            dc,
            dirty.left,
            dirty.top,
            width,
            height,
            ov.dimmedDC.Get(),
            dirty.left,
            dirty.top,
            SRCCOPY
        );

        if (!IsRectEmpty(&ov.selection)) {
            RECT bright;
            if (IntersectRect(&bright, &ov.selection, &dirty)) {
                BitBlt(
                    dc, bright.left,
                    bright.top,
                    bright.right - bright.left,
                    bright.bottom - bright.top,
                    ov.shotDC.Get(),
                    bright.left,
                    bright.top,
                    SRCCOPY
                );
            }
            DrawBorder(
                dc, ov.selection,
                BorderWidth(
                    DpiAt(ov, {ov.selection.left, ov.selection.top})
                )
            );
            DrawTextBox(dc, SizeLabel(dc, ov, ov.selection));
        }
        if (ov.showHint) {
            DrawTextBox(dc, Hint(dc, ov));
        }

        BitBlt(target, dirty.left, dirty.top, width, height, dc, dirty.left, dirty.top, SRCCOPY);
        SelectObject(dc, previous);
        DeleteObject(buffer);
        DeleteDC(dc);
    }
    EndPaint(hwnd, &ps);
}

void Finish(HWND hwnd, Overlay& ov, std::optional<RECT> result) {
    ov.result = result;
    DestroyWindow(hwnd);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* ov = reinterpret_cast<Overlay*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (!ov) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    switch (message) {
    case WM_LBUTTONDOWN:
        ov->pressed = true;
        ov->anchor = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        SetCapture(hwnd);
        return 0;

    case WM_MOUSEMOVE: {
        if (!ov->pressed) {
            return 0;
        }
        const POINT p{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

        if (!ov->dragging) {
            //  Ignore small wobbles, so a slightly shaky click still counts as a click.
            if (
                (std::abs(p.x - ov->anchor.x) < GetSystemMetrics(SM_CXDRAG)) &&
                (std::abs(p.y - ov->anchor.y) < GetSystemMetrics(SM_CYDRAG))
            ) {
                return 0;
            }
            ov->dragging = true;
            HideHint(hwnd, *ov);
        }

        SetSelection(
            hwnd, *ov,
            {
                std::min(ov->anchor.x, p.x), std::min(ov->anchor.y, p.y),
                std::max(ov->anchor.x, p.x), std::max(ov->anchor.y, p.y)
            }
        );

        return 0;
    }

    case WM_LBUTTONUP: {
        if (!ov->pressed) return 0;
        const bool dragged = ov->dragging;
        ov->pressed = ov->dragging = false;

        ReleaseCapture();

        if (!dragged) {
            Finish(hwnd, *ov, MonitorRectAt(*ov, ov->anchor));
        }
        else {
            if (!IsRectEmpty(&ov->selection)) {
                Finish(hwnd, *ov, ov->selection);
            }
        }
        //  A zero-width or zero-height drag selects nothing; let the user try again.

        return 0;
    }

    case WM_CAPTURECHANGED:
        //  Something took the mouse away mid-drag (e.g. Alt+Tab): drop the half-made selection.
        if (ov->pressed) {
            ov->pressed = ov->dragging = false;
            SetSelection(hwnd, *ov, {});
        }
        return 0;

    case WM_RBUTTONUP:
        Finish(hwnd, *ov, std::nullopt);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            Finish(hwnd, *ov, std::nullopt);
        }
        return 0;

    case WM_PAINT:
        Paint(hwnd, *ov);
        return 0;

    case WM_ERASEBKGND:
        return 1; // WM_PAINT covers every pixel

    case WM_DPICHANGED:
        //  Ignore the suggested new size: the window spans monitors with different
        //  scaling on purpose and must stay exactly on top of the screenshot.
        return 0;

    case WM_DESTROY:
        ov->done = true;
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace

std::optional<RECT> SelectRegion(HINSTANCE instance, const Screenshot& shot) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    wc.lpszClassName = kWindowClass;

    //  Already registered from an earlier capture is fine: the tray app captures many times.
    if (!RegisterClassExW(&wc) && (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)) {
        ThrowWin32Error("Registering the overlay window class");
    }

    Overlay ov(shot);
    POINT cursor{};
    GetCursorPos(&cursor);
    ov.hintPoint = {cursor.x - shot.origin.x, cursor.y - shot.origin.y};

    //  Borderless, topmost and exactly the size of the whole desktop, so its client pixels
    //  line up 1:1 with screenshot pixels. WS_EX_TOOLWINDOW keeps it off the taskbar.
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kWindowClass, L"Screenshot",
        WS_POPUP, shot.origin.x, shot.origin.y, shot.image.Width(),
        shot.image.Height(), nullptr, nullptr, instance, &ov
    );

    if (!hwnd) {
        ThrowWin32Error("Creating the overlay window");
    }

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);

    MSG msg{};
    while (!ov.done) {
        const BOOL got = GetMessageW(&msg, nullptr, 0, 0);

        if (got <= 0) {
            //  WM_QUIT arrived mid-capture. Put it back so the app's own message loop sees
            //  it too, then close the overlay.
            if (got == 0) {
                PostQuitMessage(static_cast<int>(msg.wParam));
            }
            DestroyWindow(hwnd);
            break;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return ov.result;
}
