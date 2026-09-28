#include "VlwGlyphs.h"
#include <algorithm>
namespace launcher {
namespace {
// VLW is big endian throughout: a 24 byte header, 28 bytes per glyph, then
// the bitmaps in glyph order (tools/build_font.py writes it).
uint32_t word(const uint8_t* p) { return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3]; }
}
bool VlwGlyphs::load(const uint8_t* data,size_t bytes) {
    count_=ascent_=descent_=0;
    if (!data || bytes<24) return false;
    const uint32_t count=word(data);
    if (count==0 || count>MaxGlyphs || bytes<24+size_t(count)*28) return false;
    size_t bitmap=24+size_t(count)*28;
    std::array<VlwGlyph,MaxGlyphs> glyphs{};
    std::array<uint32_t,MaxGlyphs> codes{};
    int ascent=0,descent=0;
    for (uint32_t i=0;i<count;++i) {
        const uint8_t* r=data+24+i*28;
        auto& g=glyphs[i];
        codes[i]=word(r);
        g.height=int(word(r+4)); g.width=int(word(r+8)); g.advance=int(word(r+12));
        g.dy=int(int32_t(word(r+16))); g.dx=int(int32_t(word(r+20)));
        if (g.width<0 || g.height<0 || g.width>1024 || g.height>1024) return false;
        const size_t size=size_t(g.width)*g.height;
        if (bytes<bitmap+size) return false;
        g.bitmap=data+bitmap; bitmap+=size;
        if (g.height>0) { ascent=std::max(ascent,g.dy); descent=std::max(descent,g.height-g.dy); }
    }
    codes_=codes; glyphs_=glyphs; count_=int(count); ascent_=ascent; descent_=descent;
    return true;
}
const VlwGlyph* VlwGlyphs::find(uint32_t code) const {
    for (int i=0;i<count_;++i) if (codes_[i]==code) return &glyphs_[i];
    return nullptr;
}
int VlwGlyphs::width(const char* text) const {
    int w=0;
    for (;*text;++text) if (const auto* g=find(static_cast<unsigned char>(*text))) w+=g->advance;
    return w;
}
}
