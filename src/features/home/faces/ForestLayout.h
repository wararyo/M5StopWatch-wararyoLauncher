#pragma once
#include "features/home/HomeInteraction.h"
#include "features/home/faces/InfoRow.h"
#include "features/home/faces/TimeGroups.h"
#include "features/home/VariantRecord.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
namespace launcher {
// Forest (docs/task10/plan-10-5.md 2, docs/forest-gradient/plan.md): sky,
// ground and a row of trees, the time in the sky and, when there is anything
// to show, an information row on the ground. Pure arithmetic over the viewport,
// the hour and what the fonts measure, so the painter and the host tests place
// and colour everything the same way.
//
// Positions are taken from docs/Images/WatchFace/Forest-Info.svg (466px, with
// information): without it the whole scenery moves 80px down and the time 32px.
inline constexpr int ForestReference=466;
inline constexpr int ForestMaxItems=2;
inline constexpr float ForestInfoShift=80;
// Every gradient runs down the screen, so each row of a shape has one colour.
// The sky fades between these rows; above, where the time sits, it is plain.
inline constexpr float ForestSkyFadeTop=192.5f,ForestSkyFadeBottom=294.5f;
// The ground starts here and fades until ForestGroundFadeBottom; below, where
// the information row sits, it is plain.
inline constexpr float ForestGroundTop=292,ForestGroundFadeBottom=341;
// A tree is an isosceles triangle standing on a horizontal base: its apex, the
// height of its base and half the base's width. Drawn in this order, back to
// front.
struct ForestTreeShape { float apexX,apexY,baseY,half; };
inline constexpr std::array<ForestTreeShape,20> ForestTrees={{
    {338.5f,245,305,15.155f},{349,261,318,14.722f},{394.5f,254,323.75f,17.754f},{161,235,319,21.651f},
    {250,211,315.25f,26.847f},{419,186,334.5f,38.105f},{206.5f,228,303.75f,19.486f},{310,243,313.5f,18.187f},
    {87,231,321.75f,23.383f},{214.5f,237,328.5f,23.816f},{324,254,324.5f,18.187f},{27.5f,198,324.75f,32.476f},
    {140.5f,229,328,25.548f},{444.5f,232,341.5f,28.146f},{288,273,330,14.722f},{60.5f,215,335,30.744f},
    {172.5f,237,333,24.682f},{377.5f,238,347.5f,28.146f},{265,234,343.5f,28.579f},{103.5f,225,345,30.744f}}};
// Aerial perspective: a tree takes on the sky of its own rows, the more the
// higher its base. A base at ForestHazeNear takes none of it, one at
// ForestHazeFar the palette's `haze`; fitted to the reference's opacities.
inline constexpr float ForestHazeNear=347,ForestHazeFar=307;

// A colour as 0-255 channels, in float so the gradients and the hourly palette
// blend without steps until the last conversion to RGB565.
struct ForestColor { float r=0,g=0,b=0; };
// The colours of one hour. Each gradient's two ends, and the information's ink.
struct ForestPalette {
    ForestColor skyTop,skyBottom;        // the plain sky under the time, the horizon
    ForestColor groundTop,groundBottom;  // the far ground, the plain ground under the row
    ForestColor treeTop,treeBottom;      // a tree's apex and base before the haze
    ForestColor ink;
    float haze=0.7f;                     // see ForestHazeFar
};
inline bool operator==(const ForestPalette& a,const ForestPalette& b) { return std::memcmp(&a,&b,sizeof(a))==0; }
inline bool operator!=(const ForestPalette& a,const ForestPalette& b) { return !(a==b); }
struct ForestPaletteKey { int hour; ForestPalette palette; };
namespace forest {
constexpr ForestColor hex(uint32_t c) { return {float(c>>16&0xff),float(c>>8&0xff),float(c&0xff)}; }
inline ForestColor mix(ForestColor a,ForestColor b,float t) {
    return {a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t};
}
inline uint16_t rgb565(ForestColor c) {
    auto channel=[](float v,int max) { return uint16_t(std::clamp(int(v*max/255.0f+0.5f),0,max)); };
    return uint16_t(channel(c.r,31)<<11|channel(c.g,63)<<5|channel(c.b,31));
}
// The day is the reference's colours. The other keys are provisional, to be
// tuned on the device (docs/forest-gradient/plan.md 7).
inline constexpr ForestPalette Day{hex(0x46c9e6),hex(0x9fd2de),hex(0xa0ae8d),hex(0x735a3d),
    hex(0x6eb823),hex(0x477419),hex(0xffffff),0.7f};
inline constexpr ForestPalette Dawn{hex(0x4f6fb0),hex(0xf0b48c),hex(0x9e8c7c),hex(0x5c4634),
    hex(0x5f8f34),hex(0x34501c),hex(0xffffff),0.6f};
inline constexpr ForestPalette Dusk{hex(0x34418a),hex(0xf2925e),hex(0x8e6c58),hex(0x4a3426),
    hex(0x4e7030),hex(0x2a3e16),hex(0xffffff),0.6f};
inline constexpr ForestPalette Night{hex(0x0a1430),hex(0x1f3558),hex(0x2e3440),hex(0x1a191c),
    hex(0x22402e),hex(0x0f2218),hex(0xffffff),0.5f};
}
// By hour of the day, ascending; the hours between two keys blend linearly,
// across midnight from the last key to the first.
inline constexpr std::array<ForestPaletteKey,6> ForestPaletteKeys={{
    {4,forest::Night},{6,forest::Dawn},{9,forest::Day},{16,forest::Day},{18,forest::Dusk},{20,forest::Night}}};
// The palette of a whole hour (0-23); an unknown hour (-1) has the noon's.
template<size_t N>
ForestPalette forestPaletteAt(const std::array<ForestPaletteKey,N>& keys,int hour) {
    if (hour<0 || hour>23) hour=12;
    size_t next=0;
    while (next<N && keys[next].hour<=hour) ++next;
    const auto& a=keys[(next+N-1)%N];
    const auto& b=keys[next%N];
    const int span=(b.hour-a.hour+23)%24+1;   // 24 for a single key
    const float t=float((hour-a.hour+24)%24)/span;
    ForestPalette p;
    p.skyTop=forest::mix(a.palette.skyTop,b.palette.skyTop,t);
    p.skyBottom=forest::mix(a.palette.skyBottom,b.palette.skyBottom,t);
    p.groundTop=forest::mix(a.palette.groundTop,b.palette.groundTop,t);
    p.groundBottom=forest::mix(a.palette.groundBottom,b.palette.groundBottom,t);
    p.treeTop=forest::mix(a.palette.treeTop,b.palette.treeTop,t);
    p.treeBottom=forest::mix(a.palette.treeBottom,b.palette.treeBottom,t);
    p.ink=forest::mix(a.palette.ink,b.palette.ink,t);
    p.haze=a.palette.haze+(b.palette.haze-a.palette.haze)*t;
    return p;
}
inline ForestPalette forestPalette(int hour) { return forestPaletteAt(ForestPaletteKeys,hour); }

// Where a tree is drawn on this viewport: its three corners, apex first, and
// how far back it stands (0 at ForestHazeNear, 1 at ForestHazeFar).
struct ForestTriangle { float ax,ay,lx,rx,by,depth; };
struct ForestMetrics {
    int timeAscent=84,timeDescent=1;       // Condensed SemiBold 120px (docs/task10/fonts.md)
    int hourWidth=99,minuteWidth=100,colonWidth=21;
    // The colon sits 15px below the middle of the digits. Raised 10px, in the
    // proportion Digital's 8 of 11.5 was chosen on the device (2026-09-26);
    // provisional until seen on the device.
    int colonLift=10;
    int smallAscent=18,smallDescent=4;     // 22px: the information row
};
struct ForestLayout {
    float scale=1;
    int left=0,top=0;       // the square the reference maps onto
    bool info=false;
    float skyFadeTop=0,skyFadeBottom=0;
    int groundTop=0;        // first row of ground
    float groundFadeTop=0,groundFadeBottom=0;
    std::array<ForestTriangle,ForestTrees.size()> trees{};
    int cx=0,timeBaseline=0;
    TimeGroups time{};
    InfoRow row{};          // the information row, on the ground
};
namespace forest {
inline float scale(const Viewport& v) { return float(std::min(v.width,v.height))/ForestReference; }
inline int px(const Viewport& v,float reference) { return int(reference*scale(v)); }
inline float fade(float y,float top,float bottom) { return std::clamp((y-top)/(bottom-top),0.0f,1.0f); }
// The colours at screen height `y` (a row's centre is row+0.5). Above its
// gradient each is exactly its top colour, below it exactly its bottom one.
inline ForestColor sky(const ForestLayout& l,const ForestPalette& p,float y) {
    return mix(p.skyTop,p.skyBottom,fade(y,l.skyFadeTop,l.skyFadeBottom));
}
inline ForestColor ground(const ForestLayout& l,const ForestPalette& p,float y) {
    return mix(p.groundTop,p.groundBottom,fade(y,l.groundFadeTop,l.groundFadeBottom));
}
inline float haze(const ForestTriangle& t,const ForestPalette& p) { return std::clamp(p.haze*t.depth,0.0f,1.0f); }
inline ForestColor tree(const ForestLayout& l,const ForestPalette& p,const ForestTriangle& t,float y) {
    const ForestColor own=mix(p.treeTop,p.treeBottom,fade(y,t.ay,t.by));
    return mix(own,sky(l,p,y),haze(t,p));
}
}
inline ForestLayout forestLayout(const Viewport& v,TimeVariant variant,bool info,const ForestMetrics& m={}) {
    ForestLayout l;
    l.scale=forest::scale(v);
    const int side=std::min(v.width,v.height);
    l.left=(v.width-side)/2; l.top=(v.height-side)/2;
    l.info=info;
    const float shift=info ? 0 : ForestInfoShift;
    auto x=[&](float r) { return l.left+r*l.scale; };
    auto y=[&](float r) { return l.top+(r+shift)*l.scale; };
    l.skyFadeTop=y(ForestSkyFadeTop); l.skyFadeBottom=y(ForestSkyFadeBottom);
    l.groundTop=int(std::lround(y(ForestGroundTop)));
    l.groundFadeTop=y(ForestGroundTop); l.groundFadeBottom=y(ForestGroundFadeBottom);
    for (size_t i=0;i<ForestTrees.size();++i) {
        const auto& t=ForestTrees[i];
        const float depth=std::max(0.0f,(ForestHazeNear-t.baseY)/(ForestHazeNear-ForestHazeFar));
        l.trees[i]={x(t.apexX),y(t.apexY),x(t.apexX-t.half),x(t.apexX+t.half),y(t.baseY),depth};
    }
    // The reference's digits are centred on 177.5 (145.5 with information);
    // the baseline follows from the digits' height.
    l.cx=v.width/2;
    const int middle=int(l.top+(info ? 145.5f : 177.5f)*l.scale);
    l.timeBaseline=middle+(m.timeAscent-m.timeDescent)/2;
    l.time=placeTimeGroups(l.cx,l.timeBaseline,variant,
        {m.timeAscent,m.timeDescent,m.hourWidth,m.minuteWidth,m.colonWidth});
    l.row=infoRow(v,int(l.top+377*l.scale),10);
    return l;
}
// The hour the palette follows, -1 while the time is unknown.
inline int forestHour(const WatchData& d) {
    const int h=d.localTime.tm_hour;
    return d.timeValid && h>=0 && h<24 ? h : -1;
}
// The list over Forest lies on the hour's plain ground under the row, the
// darkest of the ground's gradient.
inline uint16_t forestListBackground(int hour) { return forest::rgb565(forestPalette(hour).groundBottom); }
// Forest's behaviour without its drawing: the variant (kept in Forest's own
// record), the battery rule, which layout applies, the hour its colours follow
// and when to draw again. A tap means nothing on Forest: the list opens with
// the swipe up or A/B.
class ForestControl {
public:
    TimeVariant variant() const { return variant_; }
    void bindPreferences(FacePreferences* prefs) { record_.bind(prefs); variant_=record_.restore(TimeVariant::HourMinute); }
    // The frame about to be drawn. The same input twice leaves the same state.
    void update(const WatchData& d) {
        items_=std::min<int>(d.background.count,ForestMaxItems);
        battery_=infoBatteryShown(d.batteryPercent,d.charging,d.chargingKnown,battery_);
        hour_=forestHour(d);
    }
    bool batteryShown() const { return battery_; }
    int items() const { return items_; }
    // The information layout: the battery or any item. The battery does not
    // count towards the two items.
    bool info() const { return battery_ || items_>0; }
    // The hour of the palette, -1 while the time is unknown. The minute and
    // second deadlines already fall on every hour.
    int hour() const { return hour_; }
    HomeOutcome handle(const HomeEvent& e) {
        HomeOutcome out;
        if (e.kind!=HomeEventKind::LongPress) return out;
        variant_=variant_==TimeVariant::HourMinute ? TimeVariant::HourMinuteSecond : TimeVariant::HourMinute;
        out.changed=true;
        out.saveFailed=!record_.save(variant_);
        return out;
    }
    TimeUs nextUpdate(TimeUs now,const WatchData& d) const {
        return variant_==TimeVariant::HourMinuteSecond ? nextSecond(now,d) : nextMinute(now,d);
    }
    BackgroundInterest backgroundInterest(const BackgroundSnapshot& s) const { return leadingItems(s,ForestMaxItems); }
private:
    TimeVariant variant_=TimeVariant::HourMinute;
    VariantRecord record_;
    bool battery_=false;
    int items_=0;
    int hour_=-1;
};
}
