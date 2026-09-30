#pragma once
#include "ui/graphics/Gfx.h"
#include <cstdint>
namespace launcher {
// What a face's text and icons are drawn over (docs/task11/plan-11-3.md 4):
// one colour (Analog's black), or a colour that changes from pixel to pixel
// (Noonish's four regions). Icons and outlines blend their coverage with the
// colour at each pixel, so nothing leaves a block of another colour behind.
class Backdrop {
public:
    virtual ~Backdrop()=default;
    // The colour the face restores at pixel (x,y), RGB565.
    virtual uint16_t at(int x,int y) const=0;
    // The one colour everywhere, when there is one.
    virtual bool solid(uint16_t&) const { return false; }
};
class SolidBackdrop final : public Backdrop {
public:
    explicit SolidBackdrop(uint16_t colour):colour_(colour) {}
    uint16_t at(int,int) const override { return colour_; }
    bool solid(uint16_t& colour) const override { colour=colour_; return true; }
private:
    uint16_t colour_;
};
// Text in `ink`: opaque on a single colour; otherwise without a background of
// its own, each glyph's coverage blended into the pixels already painted
// (LovyanGFX reads them back), so the backdrop shows through around it.
inline void setTextInk(Gfx& g,uint16_t ink,const Backdrop& backdrop) {
    uint16_t colour;
    if (backdrop.solid(colour)) g.setTextColor(ink,colour);
    else g.setTextColor(ink);
}
}
