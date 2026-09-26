#pragma once
#include "ui/graphics/Gfx.h"
namespace launcher {
// An antialiased line of radius `r` with round ends, drawn only inside the
// current clip and blended with what is already there. It is LovyanGFX's
// drawWideLine (same distance, same thresholds) without its clip handling:
// that one clips to its own bounding box and clears the clip afterwards, so
// a line and whatever follows it would escape the frame's damage and blend
// a second time over the previous frame (docs/task10/plan-10-4.md 3).
void drawWideLineClipped(Gfx& g,float ax,float ay,float bx,float by,float r,uint16_t color);
}
