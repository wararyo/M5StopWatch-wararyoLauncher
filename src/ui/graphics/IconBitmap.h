#pragma once
#include <cstdint>
namespace launcher {
// An 8 bit coverage mask, width x height bytes: 0 is the background and 255
// the glyph, in whatever colour the drawer picks (pushGrayscaleImage over a
// filled circle in the list). Only a view: the pixels belong to whoever
// resolved it (the embedded icon set in assets/AppIcons.h).
struct IconBitmap { const uint8_t* pixels=nullptr; int width=0,height=0; };
}
