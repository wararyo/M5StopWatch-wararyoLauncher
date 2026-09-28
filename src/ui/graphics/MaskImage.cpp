#include "MaskImage.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace launcher {
bool fitMask(const IconBitmap& mask,int w,int h,uint8_t* out) {
    if (!mask.pixels || mask.width<=0 || mask.height<=0 || w<=0 || h<=0 || !out) return false;
    std::memset(out,0,size_t(w)*h);
    const float scale=std::min(float(w)/mask.width,float(h)/mask.height);
    const int fw=std::clamp(int(mask.width*scale+0.5f),1,w),fh=std::clamp(int(mask.height*scale+0.5f),1,h);
    const int left=(w-fw)/2,top=(h-fh)/2;
    // Source pixels per output pixel, per axis.
    const float sx=float(mask.width)/fw,sy=float(mask.height)/fh;
    for (int oy=0;oy<fh;++oy) {
        const float y0=oy*sy,y1=y0+sy;
        for (int ox=0;ox<fw;++ox) {
            const float x0=ox*sx,x1=x0+sx;
            float sum=0,area=0;
            for (int y=int(y0);y<mask.height && y<y1;++y) {
                const float wy=std::min(y1,float(y+1))-std::max(y0,float(y));
                if (wy<=0) continue;
                for (int x=int(x0);x<mask.width && x<x1;++x) {
                    const float wx=std::min(x1,float(x+1))-std::max(x0,float(x));
                    if (wx<=0) continue;
                    sum+=mask.pixels[y*mask.width+x]*wx*wy; area+=wx*wy;
                }
            }
            out[(top+oy)*w+left+ox]=uint8_t(std::min(255.0f,area>0 ? sum/area+0.5f : 0.0f));
        }
    }
    return true;
}
uint16_t blend565(uint16_t fg,uint16_t bg,uint8_t a) {
    if (a==255) return fg;
    if (a==0) return bg;
    auto mix=[a](int f,int b) { return (f*a+b*(255-a)+127)/255; };
    const int r=mix(fg>>11,bg>>11),g=mix((fg>>5)&63,(bg>>5)&63),b=mix(fg&31,bg&31);
    return uint16_t((r<<11)|(g<<5)|b);
}
}
