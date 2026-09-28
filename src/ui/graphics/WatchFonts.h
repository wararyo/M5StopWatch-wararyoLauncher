#pragma once
#include "ui/graphics/Gfx.h"
#include "ui/graphics/VlwGlyphs.h"
namespace launcher {
// The watch faces' D-DIN-PRO subsets (docs/task10/fonts.md), shared by every
// face. Each is loaded on first use and reported once; nullptr means the asset
// could not be read and the face falls back to a built-in font.
//
// The time digits are glyph sets drawn with drawGlyphs, not fonts: see
// VlwGlyphs for why.
const VlwGlyphs* digitalTimeGlyphs();  // Exp SemiBold 100px: 0-9 : -
const VlwGlyphs* forestTimeGlyphs();   // Condensed SemiBold 120px: 0-9 : -
const lgfx::IFont* watchTextFont();    // Exp Bold 28px, printable ASCII
const lgfx::IFont* watchSmallFont();   // Exp Bold 22px, printable ASCII
// Draws ASCII `text` with its first advance starting at x, on the baseline y,
// each glyph's coverage blended from `bg` to `fg` over its whole box. A
// character the set lacks is skipped.
void drawGlyphs(Gfx& g,const VlwGlyphs& glyphs,const char* text,int x,int y,uint16_t fg,uint16_t bg);
}
