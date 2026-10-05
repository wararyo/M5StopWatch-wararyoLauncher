#include "Shapes.h"
#include <algorithm>
#include <cmath>
namespace launcher {
namespace {
// LovyanGFX's thresholds: fainter than this is skipped, stronger is solid.
constexpr float Low=1.0f/32,High=1-Low;
}
void drawWideLineClipped(Gfx& g,float ax,float ay,float bx,float by,float r,uint16_t color) {
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
void fillSmoothTriangleClipped(Gfx& g,float x0,float y0,float x1,float y1,float x2,float y2,uint16_t color) {
    fillSmoothTriangleClipped(g,x0,y0,x1,y1,x2,y2,
        [](const void* c,int) { return *static_cast<const uint16_t*>(c); },&color);
}
void fillSmoothTriangleClipped(Gfx& g,float x0,float y0,float x1,float y1,float x2,float y2,RowColor rowColor,const void* context) {
    // Each edge as a*x + b*y + c: the signed distance of a point, positive
    // inside. A pixel's coverage is its distance to the nearest edge plus a
    // half, clamped to 0..1.
    struct Edge { float a,b,c; };
    const float px[3]={x0,x1,x2},py[3]={y0,y1,y2};
    Edge edges[3];
    for (int i=0;i<3;++i) {
        const int j=(i+1)%3,k=(i+2)%3;
        float a=py[j]-py[i],b=px[i]-px[j];
        const float length=std::sqrt(a*a+b*b);
        if (length<1e-6f) return;
        a/=length; b/=length;
        float c=-(a*px[i]+b*py[i]);
        if (a*px[k]+b*py[k]+c<0) { a=-a; b=-b; c=-c; }
        edges[i]={a,b,c};
    }
    int32_t cx,cy,cw,ch;
    g.getClipRect(&cx,&cy,&cw,&ch);
    const float minX=std::min({x0,x1,x2}),maxX=std::max({x0,x1,x2});
    const float minY=std::min({y0,y1,y2}),maxY=std::max({y0,y1,y2});
    const int left=int(std::floor(minX-1)),right=int(std::ceil(maxX+1));
    const int top=std::max<int>(cy,int(std::floor(minY-1))),bottom=std::min<int>(cy+ch-1,int(std::ceil(maxY+1)));
    const int clipLeft=std::max<int>(cx,left),clipRight=std::min<int>(cx+cw-1,right);
    if (clipLeft>clipRight) return;
    for (int y=top;y<=bottom;++y) {
        // The pixels any edge could cover, and those every edge covers fully,
        // from the row's constraints. Decided per row, never per clip.
        float anyLo=float(left),anyHi=float(right),fullLo=float(left),fullHi=float(right);
        bool none=false,noFull=false;
        for (const auto& e:edges) {
            const float k=e.b*y+e.c;
            if (std::fabs(e.a)<1e-6f) {
                if (k+0.5f<=Low) none=true;
                if (k+0.5f<=High) noFull=true;
                continue;
            }
            const float a=(Low-0.5f-k)/e.a,f=(High-0.5f-k)/e.a;
            if (e.a>0) { anyLo=std::max(anyLo,a); fullLo=std::max(fullLo,f); }
            else { anyHi=std::min(anyHi,a); fullHi=std::min(fullHi,f); }
        }
        if (none) continue;
        const uint16_t color=rowColor(context,y);
        const int from=std::max(clipLeft,int(std::floor(anyLo))),to=std::min(clipRight,int(std::ceil(anyHi)));
        // Strictly inside the solid bounds, so a run never holds a pixel the
        // per-pixel test would blend.
        int solidFrom=int(std::floor(fullLo))+1,solidTo=int(std::ceil(fullHi))-1;
        if (noFull) { solidFrom=1; solidTo=0; }
        auto blend=[&](int x) {
            float d=1e9f;
            for (const auto& e:edges) d=std::min(d,e.a*x+e.b*y+e.c);
            const float alpha=std::min(1.0f,d+0.5f);
            if (alpha<=Low) return;
            if (alpha>High) g.drawPixel(x,y,color);
            else g.fillRectAlpha(x,y,1,1,uint8_t(alpha*255),color);
        };
        for (int x=from;x<=to;) {
            if (x>=solidFrom && x<=solidTo) {
                const int end=std::min(to,solidTo);
                g.fillRect(x,y,end-x+1,1,color);
                x=end+1;
            } else blend(x++);
        }
    }
}
}
