#include "AnalogWatchFace.h"
#include "ui/graphics/Shapes.h"
namespace launcher {
void AnalogWatchFace::paintDot(Gfx& g,const AnalogStroke& s) {
    drawWideLineClipped(g,s.a.x,s.a.y,s.b.x,s.b.y,s.r,Orange);
}
}
