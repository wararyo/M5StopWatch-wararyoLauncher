#pragma once
#include "WatchFace.h"
#include <cstddef>
#include <cstdint>
namespace launcher {
// Bounded UTF-8 decoding; unsupported/malformed characters become '?'.
// Width truncation always removes complete characters.
void fitText(Gfx& gfx,const char* source,char* output,size_t capacity,int width);
}
