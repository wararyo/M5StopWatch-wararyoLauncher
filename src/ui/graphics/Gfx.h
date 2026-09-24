#pragma once
#include <M5GFX.h>
namespace launcher {
// The surface every face, layer and shared UI part paints on. Kept apart from
// WatchFace so the list and the toast can draw without knowing about faces.
using Gfx=m5gfx::LovyanGFX;
}
