#pragma once
#include "ui/Geometry.h"
namespace launcher {
struct Viewport {
    int width=468,height=468;
};
struct DrawRegion {
    Viewport viewport{};
    int offsetY=0;
    Rect clip{};
};
}
