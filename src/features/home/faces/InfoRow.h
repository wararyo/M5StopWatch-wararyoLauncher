#pragma once
#include "ui/rendering/Viewport.h"
#include <algorithm>
#include <cmath>
namespace launcher {
// The information row Forest, Analog and Noonish share: the battery when it
// shows, then up to two background items, each an icon and its label, centred
// as one row. Sizes follow the 466px references (docs/Images/WatchFace/
// Forest-Info.png, Analog-1.png, Noonish-Info.png), which space the row alike;
// only its height differs, and that is the face's (docs/task11/plan.md 2.2).
inline constexpr int InfoRowReference=466;
// The battery and two items.
inline constexpr int InfoRowMaxGroups=3;
struct InfoRow {
    int cx=0,y=0;           // centre of the row
    int iconSize=0,batteryWidth=0,batteryHeight=0,iconGap=0,groupGap=0;
    int width=0;            // the most the whole row may use
};
inline InfoRow infoRow(const Viewport& v,int y) {
    const float scale=float(std::min(v.width,v.height))/InfoRowReference;
    auto px=[&](float reference) { return int(reference*scale); };
    InfoRow r;
    r.cx=v.width/2; r.y=y;
    r.iconSize=px(28); r.batteryWidth=px(18); r.batteryHeight=px(30);
    r.iconGap=px(12); r.groupGap=px(28);
    // The widest the row may be where it meets the circle at its lower edge,
    // keeping a margin from the rim.
    const float radius=std::min(v.width,v.height)/2.0f;
    const float far=std::abs(y+r.iconSize/2.0f-v.height/2.0f);
    r.width=std::max(0,int(2*std::sqrt(std::max(0.0f,radius*radius-far*far)))-2*px(12));
    return r;
}
// `widths` are each group's width (icon, gap, label); returns how many were
// placed.
inline int placeInfoRow(const InfoRow& r,const int* widths,int count,Rect* out) {
    int total=count>0 ? (count-1)*r.groupGap : 0;
    for (int i=0;i<count;++i) total+=widths[i];
    int x=r.cx-total/2;
    const int h=std::max(r.iconSize,r.batteryHeight);
    for (int i=0;i<count;++i) { out[i]={x,r.y-h/2,widths[i],h}; x+=widths[i]+r.groupGap; }
    return count;
}
// The widest one group may be when `count` share the row equally.
inline int infoGroupWidthLimit(const InfoRow& r,int count) {
    count=std::max(1,count);
    return (r.width-(count-1)*r.groupGap)/count;
}
// Each group's limit from the widths it wants: narrower groups keep their
// width and leave the rest to the wider ones, so a label is shortened only
// when the row as a whole cannot hold it. Never less than the equal share.
// At most InfoRowMaxGroups.
inline void infoGroupLimits(const InfoRow& r,const int* natural,int count,int* limits) {
    count=std::min(count,InfoRowMaxGroups);
    if (count<=0) return;
    int room=r.width-(count-1)*r.groupGap;
    bool done[InfoRowMaxGroups]{};
    for (int left=count;left>0;--left) {
        // The narrowest group still open: if it fits its share it keeps its
        // width and gives the rest back; if not, neither does any wider one,
        // and all that are left split the room equally.
        int pick=-1;
        for (int i=0;i<count;++i) if (!done[i] && (pick<0 || natural[i]<natural[pick])) pick=i;
        const int share=room/left;
        if (natural[pick]>share) {
            for (int i=0;i<count;++i) if (!done[i]) limits[i]=share;
            return;
        }
        limits[pick]=share;
        room-=natural[pick];
        done[pick]=true;
    }
}
// Whether the row shows the battery (docs/task10/plan.md 3.3): below 30% or
// charging, with no hysteresis. A reading that failed is never taken for a low
// battery: an unknown level shows only while charging is confirmed or while
// the battery was already showing (then as unknown), and a charging state that
// could not be read keeps whatever was shown unless the level decides.
inline bool infoBatteryShown(int percent,bool charging,bool chargingKnown,bool wasShown) {
    if (chargingKnown && charging) return true;
    if (percent>=0 && percent<=100) {
        if (percent<30) return true;
        return chargingKnown ? false : wasShown;
    }
    return wasShown;
}
}
