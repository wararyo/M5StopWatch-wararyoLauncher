#include "WatchFonts.h"
#include "ui/graphics/VlwFont.h"
#include <cstdio>

extern const uint8_t digitalTimeStart[] asm("_binary_DDinProExpSemiBold100_vlw_start");
extern const uint8_t digitalTimeEnd[] asm("_binary_DDinProExpSemiBold100_vlw_end");
extern const uint8_t forestTimeStart[] asm("_binary_DDinProCondensedSemiBold120_vlw_start");
extern const uint8_t forestTimeEnd[] asm("_binary_DDinProCondensedSemiBold120_vlw_end");
extern const uint8_t textStart[] asm("_binary_DDinProExpBold28_vlw_start");
extern const uint8_t textEnd[] asm("_binary_DDinProExpBold28_vlw_end");
extern const uint8_t smallStart[] asm("_binary_DDinProExpBold22_vlw_start");
extern const uint8_t smallEnd[] asm("_binary_DDinProExpBold22_vlw_end");

namespace launcher {
namespace {
const VlwGlyphs* loadGlyphs(VlwGlyphs& glyphs,const uint8_t* start,const uint8_t* end,const char* name) {
    const auto bytes=size_t(end-start);
    if (!glyphs.load(start,bytes)) { std::printf("[Font] %s glyphs rejected: bytes=%u\n",name,unsigned(bytes)); return nullptr; }
    std::printf("[Font] %s glyphs ascent=%d descent=%d bytes=%u\n",name,glyphs.ascent(),glyphs.descent(),unsigned(bytes));
    return &glyphs;
}
}
const VlwGlyphs* digitalTimeGlyphs() {
    static VlwGlyphs glyphs;
    static const VlwGlyphs* loaded=loadGlyphs(glyphs,digitalTimeStart,digitalTimeEnd,"DDinProExpSemiBold100");
    return loaded;
}
const VlwGlyphs* forestTimeGlyphs() {
    static VlwGlyphs glyphs;
    static const VlwGlyphs* loaded=loadGlyphs(glyphs,forestTimeStart,forestTimeEnd,"DDinProCondensedSemiBold120");
    return loaded;
}
const lgfx::IFont* watchTextFont() {
    static EmbeddedVlw asset;
    static const lgfx::IFont* loaded=loadEmbeddedVlw(asset,textStart,textEnd,"DDinProExpBold28");
    return loaded;
}
const lgfx::IFont* watchSmallFont() {
    static EmbeddedVlw asset;
    static const lgfx::IFont* loaded=loadEmbeddedVlw(asset,smallStart,smallEnd,"DDinProExpBold22");
    return loaded;
}
void drawGlyphs(Gfx& g,const VlwGlyphs& glyphs,const char* text,int x,int y,uint16_t fg,uint16_t bg) {
    for (;*text;++text) {
        const auto* glyph=glyphs.find(static_cast<unsigned char>(*text));
        if (!glyph) continue;
        if (glyph->width>0 && glyph->height>0)
            g.pushGrayscaleImage(x+glyph->dx,y-glyph->dy,glyph->width,glyph->height,glyph->bitmap,
                                 lgfx::grayscale_8bit,fg,bg);
        x+=glyph->advance;
    }
}
}
