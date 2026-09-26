#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace launcher {
// One glyph of a VLW, read in place: the bitmap points into the asset.
struct VlwGlyph {
    const uint8_t* bitmap=nullptr;  // width x height 8 bit coverage
    int width=0,height=0,advance=0;
    int dx=0,dy=0;                  // from the pen: right, and up from the baseline
};
// A small VLW (the watch faces' time digits) indexed straight from its bytes.
// LovyanGFX draws a VLW glyph by copying its whole bitmap onto the stack
// (alloca), which for a 100px digit is several KiB of the 8KiB UI task; this
// reader lets the bitmap be pushed from where it lies instead
// (docs/task10/plan-10-3.md 4). No display types, so the host tests read the
// committed assets with it.
class VlwGlyphs {
public:
    static constexpr int MaxGlyphs=16;
    // False, leaving nothing loaded, for a truncated or oversized asset.
    bool load(const uint8_t* data,size_t bytes);
    bool loaded() const { return count_>0; }
    const VlwGlyph* find(uint32_t code) const;
    // The most any glyph reaches above and below the baseline.
    int ascent() const { return ascent_; }
    int descent() const { return descent_; }
    // Sum of the advances; a character the asset lacks adds nothing.
    int width(const char* text) const;
private:
    std::array<uint32_t,MaxGlyphs> codes_{};
    std::array<VlwGlyph,MaxGlyphs> glyphs_{};
    int count_=0,ascent_=0,descent_=0;
};
}
