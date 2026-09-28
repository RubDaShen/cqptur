#pragma once

#include <windows.h>

#include <cstdint>

//  A 32-bit top-down GDI bitmap whose pixels can also be read directly.
class DibSection {
//	|-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-|
//				    Members and Fields
//	|-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-|

    //	-------------------------------------------
    //			        Variables
    //	-------------------------------------------

private:
    int width_;
    int height_;
    HBITMAP bitmap_;
    uint32_t* pixels_;

//	|-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-|
//			        Functions and Methods
//	|-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-|

    //	-------------------------------------------
    //		    Constructors and Destructors
    //	-------------------------------------------

public:
    DibSection(int width, int height);
    DibSection(DibSection&& other) noexcept;
    DibSection(const DibSection&) = delete;

    ~DibSection();

    //	-------------------------------------------
    //			        Functions
    //	-------------------------------------------    

public:
    DibSection& operator=(const DibSection&) = delete;
    DibSection& operator=(DibSection&&) = delete;
    
    int Width() const { return width_; }
    int Height() const { return height_; }
    HBITMAP Handle() const { return bitmap_; }

    //  One uint32_t per pixel (0x00RRGGBB, i.e. BGRX in memory), rows top to bottom.
    uint32_t* Pixels() {
        return pixels_;
    }
    const uint32_t* Pixels() const {
        return pixels_;
    }
};

//  A frozen copy of every monitor at its native (physical) resolution.
struct Screenshot {
    //  Desktop position of pixel (0, 0). Negative when a monitor sits left of or above
    //  the primary one.
    POINT origin;
    DibSection image;
};

Screenshot CaptureVirtualScreen();
