#include "output.h"

#include "error.h"

#include <objbase.h>
#include <shlobj.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <string>

using Microsoft::WRL::ComPtr;

namespace {

//  Pixel density written into the files. 96 DPI means "one image pixel = one pixel", so apps
//  that honor the tag (Word, OneNote...) don't shrink the image by the display scale.
constexpr double kDpi = 96.0;
constexpr LONG kPixelsPerMeter = 3780;  // 96 DPI

//  Converts to a packed DIB (header + pixels), the format CF_DIB expects. It uses 24 bits per
//  pixel so no app mistakes the unused fourth byte for transparency.
std::vector<uint8_t> MakeDib(const Image& image) {
    const size_t stride = (size_t(image.width) * 3 + 3) & ~size_t(3); // rows are 4-byte aligned
    std::vector<uint8_t> dib(sizeof(BITMAPINFOHEADER) + stride * image.height);

    BITMAPINFOHEADER header{};
    header.biSize = sizeof(header);
    header.biWidth = image.width;
    header.biHeight = image.height; // positive = bottom-up rows, the most widely supported layout
    header.biPlanes = 1;
    header.biBitCount = 24;
    header.biCompression = BI_RGB;
    header.biSizeImage = static_cast<DWORD>(stride * image.height);
    header.biXPelsPerMeter = kPixelsPerMeter;
    header.biYPelsPerMeter = kPixelsPerMeter;
    std::memcpy(dib.data(), &header, sizeof(header));

    uint8_t* bits = dib.data() + sizeof(header);
    for (int y = 0; y < image.height; ++y) {
        const uint32_t* src = image.pixels.data() + size_t(image.height - 1 - y) * image.width;
        uint8_t* dst = bits + y * stride;
        for (int x = 0; x < image.width; ++x) {
            dst[3 * x + 0] = static_cast<uint8_t>(src[x]);        // blue
            dst[3 * x + 1] = static_cast<uint8_t>(src[x] >> 8);   // green
            dst[3 * x + 2] = static_cast<uint8_t>(src[x] >> 16);  // red
        }
    }
    return dib;
}

//  On success the clipboard takes ownership of the memory; on failure we free it.
bool PutOnClipboard(UINT format, const std::vector<uint8_t>& bytes) {
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
    if (!memory) return false;
    std::memcpy(GlobalLock(memory), bytes.data(), bytes.size());
    GlobalUnlock(memory);
    if (!SetClipboardData(format, memory)) {
        GlobalFree(memory);
        return false;
    }
    return true;
}

}  // namespace

Image Crop(const DibSection& source, const RECT& area) {
    const RECT bounds{0, 0, source.Width(), source.Height()};
    RECT r{};
    IntersectRect(&r, &area, &bounds);

    Image image{r.right - r.left, r.bottom - r.top, {}};
    image.pixels.resize(size_t(image.width) * image.height);
    for (int y = 0; y < image.height; ++y) {
        const uint32_t* row = source.Pixels() + size_t(r.top + y) * source.Width() + r.left;
        std::copy_n(row, image.width, image.pixels.begin() + ptrdiff_t(y) * image.width);
    }
    return image;
}

std::vector<uint8_t> EncodePng(const Image& image) {
    ComPtr<IWICImagingFactory> factory;
    ThrowIfFailed(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&factory)),
                  "Starting the image encoder");

    ComPtr<IWICBitmap> bitmap;
    ThrowIfFailed(factory->CreateBitmapFromMemory(
                      image.width, image.height, GUID_WICPixelFormat32bppBGR, image.width * 4,
                      static_cast<UINT>(image.pixels.size() * 4),
                      reinterpret_cast<BYTE*>(const_cast<uint32_t*>(image.pixels.data())), &bitmap),
                  "Preparing the image");

    ComPtr<IStream> stream;
    ThrowIfFailed(CreateStreamOnHGlobal(nullptr, TRUE, &stream), "Allocating the PNG buffer");

    ComPtr<IWICBitmapEncoder> encoder;
    ThrowIfFailed(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder),
                  "Creating the PNG encoder");
    ThrowIfFailed(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache), "Encoding the PNG");

    ComPtr<IWICBitmapFrameEncode> frame;
    ThrowIfFailed(encoder->CreateNewFrame(&frame, nullptr), "Encoding the PNG");
    ThrowIfFailed(frame->Initialize(nullptr), "Encoding the PNG");
    ThrowIfFailed(frame->SetSize(image.width, image.height), "Encoding the PNG");
    ThrowIfFailed(frame->SetResolution(kDpi, kDpi), "Encoding the PNG");
    WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;  // plain RGB PNG, no alpha channel
    ThrowIfFailed(frame->SetPixelFormat(&format), "Encoding the PNG");
    ThrowIfFailed(frame->WriteSource(bitmap.Get(), nullptr), "Encoding the PNG");
    ThrowIfFailed(frame->Commit(), "Encoding the PNG");
    ThrowIfFailed(encoder->Commit(), "Encoding the PNG");

    STATSTG stat{};
    ThrowIfFailed(stream->Stat(&stat, STATFLAG_NONAME), "Reading the PNG");
    std::vector<uint8_t> png(static_cast<size_t>(stat.cbSize.QuadPart));
    ThrowIfFailed(stream->Seek({}, STREAM_SEEK_SET, nullptr), "Reading the PNG");
    ULONG read = 0;
    ThrowIfFailed(stream->Read(png.data(), static_cast<ULONG>(png.size()), &read), "Reading the PNG");
    png.resize(read);
    return png;
}

void CopyToClipboard(const Image& image, const std::vector<uint8_t>& png) {
    const std::vector<uint8_t> dib = MakeDib(image);

    //  The clipboard needs an owner window; a hidden message-only one will do.
    HWND owner = CreateWindowExW(0, L"STATIC", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                 nullptr, nullptr);
    if (!owner) ThrowWin32Error("Creating the clipboard window");

    //  Another app may be holding the clipboard for a moment, so retry briefly.
    bool opened = false;
    for (int attempt = 0; attempt < 10 && !opened; ++attempt) {
        opened = OpenClipboard(owner);
        if (!opened) Sleep(20);
    }
    if (!opened) {
        const DWORD error = GetLastError();
        DestroyWindow(owner);
        ThrowWin32Error("Opening the clipboard", error);
    }

    EmptyClipboard();
    //  PNG first: browsers, Office and chat apps prefer it. CF_DIB is the classic fallback.
    const bool pngOk = PutOnClipboard(RegisterClipboardFormatW(L"PNG"), png);
    const bool dibOk = PutOnClipboard(CF_DIB, dib);
    CloseClipboard();
    DestroyWindow(owner);
    if (!pngOk && !dibOk) ThrowWin32Error("Copying to the clipboard");
}

std::filesystem::path SaveToScreenshotsFolder(const std::vector<uint8_t>& png) {
    PWSTR folderPath = nullptr;
    const HRESULT hr =
        SHGetKnownFolderPath(FOLDERID_Screenshots, KF_FLAG_CREATE, nullptr, &folderPath);
    const std::filesystem::path folder = SUCCEEDED(hr) ? folderPath : L"";
    CoTaskMemFree(folderPath);
    ThrowIfFailed(hr, "Finding the Screenshots folder");

    SYSTEMTIME now;
    GetLocalTime(&now);
    const std::wstring stem = std::format(L"Screenshot {:04}-{:02}-{:02} {:02}{:02}{:02}", now.wYear,
                                          now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);

    for (int n = 1;; ++n) {
        const std::filesystem::path path =
            folder / (n == 1 ? stem + L".png" : std::format(L"{} ({}).png", stem, n));
        //  CREATE_NEW never overwrites: if the name is taken, try the next one.
        HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_FILE_EXISTS) continue;
            ThrowWin32Error("Creating the screenshot file");
        }

        DWORD written = 0;
        const BOOL ok =
            WriteFile(file, png.data(), static_cast<DWORD>(png.size()), &written, nullptr);
        const DWORD error = GetLastError();
        CloseHandle(file);
        if (!ok || written != png.size()) {
            DeleteFileW(path.c_str());
            ThrowWin32Error("Writing the screenshot file", error);
        }
        return path;
    }
}
