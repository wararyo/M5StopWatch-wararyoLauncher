#pragma once
#include <cstdint>
namespace launcher {
// An 8 bit coverage mask drawn with pushGrayscaleImage over an already filled
// circle, so 0 keeps the circle colour and 255 is the glyph. Only a view: the
// pixels belong to whoever resolved it (the launcher's embedded icon set).
struct IconBitmap { const uint8_t* pixels=nullptr; int width=0,height=0; };
}
