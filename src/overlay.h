#pragma once

#include "capture.h"

#include <optional>

//  Shows the frozen screenshot across all monitors and lets the user choose what to keep:
//  drag to select an area, click to take the whole monitor under the cursor, Esc or
//  right-click to cancel. Returns the chosen area in screenshot pixels, or nothing if
//  the user cancelled.
std::optional<RECT> SelectRegion(HINSTANCE instance, const Screenshot& shot);
