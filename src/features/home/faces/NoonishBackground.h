#pragma once
#include "features/home/faces/AnalogLayout.h"
#include "ui/graphics/MaskImage.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
namespace launcher {
// Noonish's background (docs/task11/plan.md 2.4, plan-11-3.md 3): the lines
// of the hour and the minute hand, drawn through the centre and on past it,
// cut the face into four regions. Which region a pixel is in comes from which
// side of each line it lies on, never from the order of the angles, so a
// region keeps its colour as the hands move and simply narrows away when they
// meet or point opposite ways.
//
// Colours from docs/Images/WatchFace/Noonish*.png, RGB565: top (96,196,218),
// right (144,214,229), bottom (131,167,209), left (78,130,189); the date and
// the row (222,243,247). With the hour hand at about ten and the minute hand
// at about half past one, as there, the region clockwise of the hour hand and
// anticlockwise of the minute hand is the top one.
enum NoonishRegion : uint8_t { NoonishTop,NoonishRight,NoonishBottom,NoonishLeft };
inline constexpr std::array<uint16_t,4> NoonishPalette={0x663b,0x96bc,0x853a,0x4c17};
inline constexpr uint16_t NoonishInk=0xdf9e;
// The dot is the backdrop lightened: this much white over it (179/255, 70%, as
// measured on the reference; 11-1 validation 2).
inline constexpr uint8_t NoonishLight=179;
// While the time is unknown the background rests at 10:07 (plan.md 2.4).
inline constexpr int NoonishRestStep=10*360+7*6;
// The two lines for one step of the hands: unit vectors along the hands.
struct NoonishSplit {
    AnalogPoint centre{};
    float hx=0,hy=-1,mx=0,my=-1;
    int step=0;     // what the background shows; not the seconds
};
inline NoonishSplit noonishSplit(const AnalogLayout& l,const AnalogTime& t) {
    NoonishSplit s;
    s.centre=l.centre;
    s.step=t.valid ? t.step : NoonishRestStep;
    AnalogTime shown; shown.valid=true; shown.step=s.step;
    const auto h=analogPoint(l,1,analogHourDegrees(shown)),m=analogPoint(l,1,analogMinuteDegrees(shown));
    s.hx=h.x-l.centre.x; s.hy=h.y-l.centre.y; s.mx=m.x-l.centre.x; s.my=m.y-l.centre.y;
    return s;
}
// The region of a point. Positive across a line is clockwise of the hand; a
// point exactly on a line counts as anticlockwise of it.
inline NoonishRegion noonishRegion(const NoonishSplit& s,float x,float y) {
    const float px=x-s.centre.x,py=y-s.centre.y;
    const bool h=s.hx*py-s.hy*px>0,m=s.mx*py-s.my*px>0;
    return h ? (m ? NoonishRight : NoonishTop) : (m ? NoonishBottom : NoonishLeft);
}
namespace noonish {
// Pixels whose centre is at least this far from both lines take one colour;
// nearer, four samples at a quarter pixel from the centre are averaged. Every
// sample lies within 0.36px of the centre, so the two agree at the threshold.
inline constexpr float Edge=0.75f;
inline constexpr float Quarter=0.25f;
}
// The colour the background shows at pixel (x,y): depends on the pixel and the
// split alone, so a partial repaint gives what a full one does.
inline uint16_t noonishColour(const NoonishSplit& s,int x,int y) {
    const float px=x-s.centre.x,py=y-s.centre.y;
    const float dh=s.hx*py-s.hy*px,dm=s.mx*py-s.my*px;
    if (std::fabs(dh)>=noonish::Edge && std::fabs(dm)>=noonish::Edge)
        return NoonishPalette[noonishRegion(s,float(x),float(y))];
    int r=0,g=0,b=0;
    for (float oy:{-noonish::Quarter,noonish::Quarter}) for (float ox:{-noonish::Quarter,noonish::Quarter}) {
        const uint16_t c=NoonishPalette[noonishRegion(s,x+ox,y+oy)];
        r+=c>>11; g+=(c>>5)&0x3f; b+=c&0x1f;
    }
    return uint16_t(((r+2)/4)<<11|((g+2)/4)<<5|((b+2)/4));
}
// The middle pixel of the panel's top row: the home gesture's band over
// Noonish, which gives way to it. Its region's colour, or the blend of two
// while a hand's line passes through it (at half past three, say), exactly
// as the background draws it there.
inline uint16_t noonishTopColour(const AnalogLayout& l,const AnalogTime& t) {
    return noonishColour(noonishSplit(l,t),int(l.centre.x),0);
}
// Row y from x0 to x1 (exclusive) as runs of one colour, left to right: the
// colours noonishColour gives pixel by pixel, found without asking every
// pixel. Only where a line comes within reach of a pixel's samples can the
// colour change, and a row meets each line once; between those places the
// region is one colour, taken from a single pixel. `run(x,length,colour)`.
template<class Run> void noonishRow(const NoonishSplit& s,int y,int x0,int x1,Run&& run) {
    // Each line's distance along the row is a+b*x; the pixels within Edge of
    // it, widened by a pixel for rounding, are asked one by one.
    const float py=y-s.centre.y;
    int spans[2][2]; int count=0;
    const float lines[2][2]={{s.hx,s.hy},{s.mx,s.my}};
    for (const auto& line:lines) {
        const float a=line[0]*py+line[1]*s.centre.x,b=-line[1];
        int lo,hi;
        if (std::fabs(b)<1e-6f) {
            if (std::fabs(a)>=noonish::Edge) continue;
            lo=x0; hi=x1-1;
        } else {
            const float p=(-noonish::Edge-a)/b,q=(noonish::Edge-a)/b;
            lo=int(std::floor(std::min(p,q)))-1; hi=int(std::ceil(std::max(p,q)))+1;
        }
        lo=std::max(lo,x0); hi=std::min(hi,x1-1);
        if (lo>hi) continue;
        spans[count][0]=lo; spans[count][1]=hi; ++count;
    }
    if (count==2 && spans[1][0]<spans[0][0]) std::swap(spans[0],spans[1]);
    if (count==2 && spans[1][0]<=spans[0][1]+1) { spans[0][1]=std::max(spans[0][1],spans[1][1]); count=1; }
    // Runs of one colour, merged across the joins between plain and asked.
    int start=x0;
    uint16_t colour=0;
    bool open=false;
    auto put=[&](int x,uint16_t c) {
        if (open && c==colour) return;
        if (open) run(start,x-start,colour);
        start=x; colour=c; open=true;
    };
    int x=x0;
    for (int i=0;i<=count;++i) {
        const int plainEnd=i<count ? spans[i][0] : x1;
        if (x<plainEnd) { put(x,noonishColour(s,x,y)); x=plainEnd; }
        if (i==count) break;
        for (;x<=spans[i][1];++x) put(x,noonishColour(s,x,y));
    }
    if (open) run(start,x1-start,colour);
}
// The dot's colour where the background is `background`.
inline uint16_t noonishLight(uint16_t background) { return blend565(0xffff,background,NoonishLight); }
// The dot at pixel (x,y): false where it does not reach. Its coverage follows
// drawWideLineClipped's rule; the light is taken from the background at that
// very pixel, so a dot across a boundary is lighter on both sides.
inline bool noonishDot(const NoonishSplit& s,const AnalogStroke& dot,int x,int y,uint16_t& out) {
    const float alpha=dot.r+0.5f-std::hypot(x-dot.a.x,y-dot.a.y);
    if (alpha<=1.0f/32) return false;
    const uint16_t background=noonishColour(s,x,y),light=noonishLight(background);
    out=alpha>31.0f/32 ? light : blend565(light,background,uint8_t(alpha*255+0.5f));
    return true;
}
}
