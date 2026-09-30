#pragma once
#include "features/home/HomeInteraction.h"
#include "features/home/faces/InfoRow.h"
#include "features/home/faces/TimeGroups.h"
#include "features/home/VariantRecord.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace launcher {
// Forest (docs/task10/plan-10-5.md 2): sky, ground and a row of trees, the time
// in the sky and, when there is anything to show, an information row on the
// ground. Pure arithmetic over the viewport and what the fonts measure, so the
// painter and the host tests place everything the same way.
//
// Positions and colours are taken from docs/Images/WatchFace/Forest*.png (466px):
// with information the whole scenery moves 80px up and the time 32px up.
inline constexpr int ForestReference=466;
inline constexpr int ForestMaxItems=2;
// RGB565 of the reference colours: sky (96,196,218), ground (147,128,98),
// trees (85,135,34).
inline constexpr uint16_t ForestSky=0x663b,ForestGround=0x940c,ForestTree=0x5424,ForestInk=0xffff;
// A tree is an isosceles triangle standing on a horizontal base: its apex and
// the height of its base, in the reference's plain layout. Its sides fall 3.9px
// for every pixel across, as measured on the reference.
struct ForestTreeShape { float apexX,apexY,baseY; };
inline constexpr float ForestTreeSlope=3.9f;
inline constexpr std::array<ForestTreeShape,9> ForestTrees={{
    {47,361,416.5f},{105,308,393.5f},{136,300,409},{179,306,381.5f},{251,302,394.5f},
    {286,322,386.5f},{336,272,411.8f},{370,330,389.5f},{405,343,398.5f}}};
inline constexpr float ForestInfoShift=80;
// Where a tree is drawn on this viewport: its three corners, apex first.
struct ForestTriangle { float ax,ay,lx,rx,by; };
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
    int groundTop=0;        // first row of ground
    std::array<ForestTriangle,ForestTrees.size()> trees{};
    int cx=0,timeBaseline=0;
    TimeGroups time{};
    InfoRow row{};          // the information row, on the ground
};
namespace forest {
inline float scale(const Viewport& v) { return float(std::min(v.width,v.height))/ForestReference; }
inline int px(const Viewport& v,float reference) { return int(reference*scale(v)); }
}
inline ForestLayout forestLayout(const Viewport& v,TimeVariant variant,bool info,const ForestMetrics& m={}) {
    ForestLayout l;
    l.scale=forest::scale(v);
    const int side=std::min(v.width,v.height);
    l.left=(v.width-side)/2; l.top=(v.height-side)/2;
    l.info=info;
    const float shift=info ? ForestInfoShift : 0;
    auto x=[&](float r) { return l.left+r*l.scale; };
    auto y=[&](float r) { return l.top+r*l.scale; };
    l.groundTop=int(std::lround(y(372-shift)));
    for (size_t i=0;i<ForestTrees.size();++i) {
        const auto& t=ForestTrees[i];
        const float half=(t.baseY-t.apexY)/ForestTreeSlope;
        l.trees[i]={x(t.apexX),y(t.apexY-shift),x(t.apexX-half),x(t.apexX+half),y(t.baseY-shift)};
    }
    // The reference's digits are centred on 177.5 (145.5 with information);
    // the baseline follows from the digits' height.
    l.cx=v.width/2;
    const int middle=int(y(info ? 145.5f : 177.5f));
    l.timeBaseline=middle+(m.timeAscent-m.timeDescent)/2;
    l.time=placeTimeGroups(l.cx,l.timeBaseline,variant,
        {m.timeAscent,m.timeDescent,m.hourWidth,m.minuteWidth,m.colonWidth});
    l.row=infoRow(v,int(y(367)));
    return l;
}
// Forest's behaviour without its drawing: the variant (kept in Forest's own
// record), the battery rule, which layout applies, and when to draw again. A
// tap means nothing on Forest: the list opens with the swipe up or A/B.
class ForestControl {
public:
    TimeVariant variant() const { return variant_; }
    void bindPreferences(FacePreferences* prefs) { record_.bind(prefs); variant_=record_.restore(TimeVariant::HourMinute); }
    // The frame about to be drawn. The same input twice leaves the same state.
    void update(const WatchData& d) {
        items_=std::min<int>(d.background.count,ForestMaxItems);
        battery_=infoBatteryShown(d.batteryPercent,d.charging,d.chargingKnown,battery_);
    }
    bool batteryShown() const { return battery_; }
    int items() const { return items_; }
    // The information layout: the battery or any item. The battery does not
    // count towards the two items.
    bool info() const { return battery_ || items_>0; }
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
};
}
