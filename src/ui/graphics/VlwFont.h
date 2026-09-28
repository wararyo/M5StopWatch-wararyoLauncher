#pragma once
#include "ui/graphics/Gfx.h"
namespace launcher {
// Embedded VLW font subset (tools/build_font.py). A VLW holds one weight at one
// size, so another face means another asset and another accessor here.
// The font object is owned here
// instead of by LovyanGFX: setFont() destroys the display's runtime font, so a
// loaded font could not be selected again after switching away.
// Returns nullptr when loading fails; callers then keep a built-in font.
const lgfx::IFont* vlwFont();
// One embedded VLW and the reader it is loaded through. The wrapper must
// outlive the font, since glyph bitmaps are read on demand, so both live in
// static storage for the whole run.
struct EmbeddedVlw {
    lgfx::PointerWrapper data;
    lgfx::VLWfont font;
};
// Loads `asset` from the linked bytes once and reports the result once;
// nullptr when the bytes are not a readable VLW.
const lgfx::IFont* loadEmbeddedVlw(EmbeddedVlw& asset,const uint8_t* start,const uint8_t* end,const char* name);
}
