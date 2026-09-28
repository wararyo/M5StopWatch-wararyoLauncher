#pragma once
#include <M5GFX.h>
namespace launcher {
// The surface every face, layer and shared UI part paints on. Kept apart from
// WatchFace so the list and the toast can draw without knowing about faces.
using Gfx=m5gfx::LovyanGFX;
// Never call Gfx::drawWideLine (nor drawSmoothLine or drawWedgeLine) in a
// frame: it replaces the clip with its own bounding box and clears it when
// done, so it paints outside the frame's damage. Use drawWideLineClipped
// (ui/graphics/Shapes.h).
}
