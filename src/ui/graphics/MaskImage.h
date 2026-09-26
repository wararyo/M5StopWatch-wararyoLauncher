#pragma once
#include "ui/graphics/IconBitmap.h"
#include <cstdint>
namespace launcher {
// Fits `mask` into a w x h coverage buffer: scaled to the largest size that
// keeps its aspect, centred, each output pixel the area average of the source
// pixels it covers. The rest of `out` (w*h bytes) is left at 0. Returns false,
// writing nothing, for an unusable mask or size. No display types: the result
// is blended by whoever draws it.
bool fitMask(const IconBitmap& mask,int w,int h,uint8_t* out);
// `fg` over `bg` at `coverage` (0..255), both RGB565.
uint16_t blend565(uint16_t fg,uint16_t bg,uint8_t coverage);
}
