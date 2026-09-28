#include "VlwFont.h"
#include <cstdio>

extern const uint8_t fontStart[] asm("_binary_GenShinGothicMedium28_vlw_start");
extern const uint8_t fontEnd[] asm("_binary_GenShinGothicMedium28_vlw_end");

namespace launcher {
const lgfx::IFont* loadEmbeddedVlw(EmbeddedVlw& asset,const uint8_t* start,const uint8_t* end,const char* name) {
    const auto bytes=static_cast<uint32_t>(end-start);
    asset.data.set(start,bytes);
    if(!asset.font.loadFont(&asset.data)) { std::printf("[Font] %s load failed\n",name); return nullptr; }
    std::printf("[Font] %s glyphs=%u bytes=%u\n",name,unsigned(asset.font.gCount),unsigned(bytes));
    return &asset.font;
}
const lgfx::IFont* vlwFont() {
    static EmbeddedVlw asset;
    static const lgfx::IFont* loaded=loadEmbeddedVlw(asset,fontStart,fontEnd,"GenShinGothicMedium28");
    return loaded;
}
}
