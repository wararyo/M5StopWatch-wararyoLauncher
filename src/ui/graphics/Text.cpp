#include "Text.h"
#include <cstring>
namespace launcher {
void fitText(Gfx& g,const char* source,char* out,size_t cap,int width) {
    if(!cap) return;
    size_t used=0; out[0]=0; bool truncated=false;
    while(*source) {
        const auto first=static_cast<unsigned char>(*source);
        int bytes=first<128 ? 1 : first>=0xc2 && first<=0xdf ? 2 :
            first>=0xe0 && first<=0xef ? 3 : first>=0xf0 && first<=0xf4 ? 4 : 1;
        uint32_t code=first & (bytes==1 ? 127 : (1<<(7-bytes))-1);
        bool valid=bytes!=1 || first<128;
        for(int i=1;i<bytes;++i) {
            const auto ch=static_cast<unsigned char>(source[i]);
            if(!ch || (ch&0xc0)!=0x80) { valid=false; bytes=i; break; }
            code=(code<<6)|(ch&63);
        }
        if((bytes==2 && code<128) || (bytes==3 && code<2048) || (bytes==4 && code<65536) ||
           code>65535 || (code>=0xd800 && code<=0xdfff)) valid=false;
        lgfx::FontMetrics metric{};
        if(valid) { g.getFont()->getDefaultMetric(&metric); valid=g.getFont()->updateFontMetric(&metric,static_cast<uint16_t>(code)); }
        const int count=valid ? bytes : 1;
        if(used+count+1>cap) { truncated=true; break; }
        if(valid) std::memcpy(out+used,source,bytes); else out[used]='?';
        used+=count; out[used]=0; source+=bytes;
        if(g.textWidth(out)>width) { truncated=true; break; }
    }
    if(!truncated) return;
    auto removeLast=[&] {
        if(!used) return;
        --used;
        while(used && (static_cast<unsigned char>(out[used])&0xc0)==0x80) --used;
        out[used]=0;
    };
    while(used && (used+4>cap || g.textWidth(out)+g.textWidth("...")>width)) removeLast();
    if(used+4<=cap && g.textWidth(out)+g.textWidth("...")<=width) std::memcpy(out+used,"...",4);
    else out[0]=0;
}
}
