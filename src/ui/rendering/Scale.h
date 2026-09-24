#pragma once
#include "ui/rendering/Viewport.h"
#include <algorithm>
namespace launcher {
inline int scaled(const Viewport& v,int px) { return std::max(1,px*std::min(v.width,v.height)/468); }
inline int offsetPx(const Viewport& v,int px) { return px*std::min(v.width,v.height)/468; }
}
