#include "Shapes.h"
#include <algorithm>
#include <cmath>
namespace launcher {
void drawWideLineClipped(Gfx& g,float ax,float ay,float bx,float by,float r,uint16_t color) {
    // LovyanGFX's thresholds: fainter than this is skipped, stronger is solid.
    constexpr float Low=1.0f/32,High=1-Low;
    if (r<0) return;
    int32_t cx,cy,cw,ch;
    g.getClipRect(&cx,&cy,&cw,&ch);
    const int x0=std::max<int>(cx,int(std::floor(std::min(ax,bx)-r-1)));
    const int x1=std::min<int>(cx+cw-1,int(std::ceil(std::max(ax,bx)+r+1)));
    const int y0=std::max<int>(cy,int(std::floor(std::min(ay,by)-r-1)));
    const int y1=std::min<int>(cy+ch-1,int(std::ceil(std::max(ay,by)+r+1)));
    const float dx=bx-ax,dy=by-ay,length=dx*dx+dy*dy;
    for (int y=y0;y<=y1;++y) for (int x=x0;x<=x1;++x) {
        const float px=x-ax,py=y-ay;
        const float t=length>0 ? std::clamp((px*dx+py*dy)/length,0.0f,1.0f) : 0.0f;
        const float ex=px-dx*t,ey=py-dy*t;
        const float alpha=r+0.5f-std::sqrt(ex*ex+ey*ey);
        if (alpha<=Low) continue;
        if (alpha>High) g.drawPixel(x,y,color);
        else g.fillRectAlpha(x,y,1,1,uint8_t(alpha*255),color);
    }
}
}
