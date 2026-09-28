#pragma once

#include "capture.h"

#include <cstdint>
#include <filesystem>
#include <vector>

struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels; // BGRX, rows top to bottom
};

Image Crop(const DibSection& source, const RECT& area);

std::vector<uint8_t> EncodePng(const Image& image);

//  Puts the image on the clipboard as both PNG and a classic bitmap, so every app can paste it.
void CopyToClipboard(const Image& image, const std::vector<uint8_t>& png);

//  Saves to the user's Screenshots folder (Pictures\Screenshots) and returns the file's path.
std::filesystem::path SaveToScreenshotsFolder(const std::vector<uint8_t>& png);
