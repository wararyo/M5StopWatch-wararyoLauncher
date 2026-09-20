#pragma once
#include "app/AppRegistry.h"
#include <cstdint>
namespace launcher {
// One app list icon: an 8 bit coverage mask drawn with pushGrayscaleImage over
// the already filled circle, so 0 keeps the circle colour and 255 is the glyph.
struct IconBitmap { const uint8_t* pixels=nullptr; int width=0,height=0; };
// Embedded set built by tools/build_icons.py from the PNGs in icons/.
// Returns nullptr when the asset is missing or malformed; the list then draws
// the circle alone instead of failing to show the row.
const IconBitmap* appIcon(IconId id);
}
