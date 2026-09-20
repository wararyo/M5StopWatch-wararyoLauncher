#pragma once
#include "WatchFace.h"
namespace launcher {
// Embedded VLW font subset (tools/build_font.py). A VLW holds one weight at one
// size, so another face means another asset and another accessor here.
// The font object is owned here
// instead of by LovyanGFX: setFont() destroys the display's runtime font, so a
// loaded font could not be selected again after switching away.
// Returns nullptr when loading fails; callers then keep a built-in font.
const lgfx::IFont* vlwFont();
}
