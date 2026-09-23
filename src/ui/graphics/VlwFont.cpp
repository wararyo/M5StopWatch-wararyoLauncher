#include "VlwFont.h"
#include <cstdio>

extern const uint8_t fontStart[] asm("_binary_GenShinGothicMedium28_vlw_start");
extern const uint8_t fontEnd[] asm("_binary_GenShinGothicMedium28_vlw_end");

namespace launcher {
const lgfx::IFont* vlwFont() {
    // The wrapper must outlive the font: glyph bitmaps are read on demand.
    static lgfx::PointerWrapper data;
    static lgfx::VLWfont font;
    static const lgfx::IFont* loaded=[]() -> const lgfx::IFont* {
        const auto bytes=static_cast<uint32_t>(fontEnd-fontStart);
        data.set(fontStart,bytes);
        if(!font.loadFont(&data)) { std::printf("[Font] GenShinGothicMedium28 load failed\n"); return nullptr; }
        std::printf("[Font] GenShinGothicMedium28 glyphs=%u bytes=%u\n",unsigned(font.gCount),unsigned(bytes));
        return &font;
    }();
    return loaded;
}
}
