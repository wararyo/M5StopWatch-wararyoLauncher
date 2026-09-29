#pragma once
#include "features/home/HomeInteraction.h"
#include "features/home/VariantRecord.h"
#include "features/home/faces/InfoRow.h"
#include <algorithm>
#include <cmath>
namespace launcher {
// Analog and Noonish (docs/task11/plan.md): the hour and minute hands from the
// centre, a dot on an orbit for the seconds, the day of the month on the right
// and the information row below. Both faces place everything here, as pure
// arithmetic over the viewport and what the fonts measure, so the painters and
// the host tests agree. Nothing moves with the seconds or the information:
// only the hands and the dot turn.
//
// Measured on docs/Images/WatchFace/Analog*.png and Noonish*.png (466px) in the
// painters' coordinates, where a pixel's centre is its integer position: the
// hands and the hub of both are symmetric about (232.5,232.5). A hand is a line
// with round ends from the centre; its length runs to the centre of its far
// end. The references' information row is Forest's, 10px higher.
inline constexpr int AnalogReference=466;
inline constexpr int AnalogMaxItems=2;
namespace analog {
inline constexpr float Centre=232.5f;
inline constexpr float HourLength=122,HourWidth=10;
inline constexpr float MinuteLength=169,MinuteWidth=5;
inline constexpr float HubRadius=12;
// The references' dot sits half a pixel right of the axis; here it turns
// about the centre.
inline constexpr float SecondOrbit=199.5f,SecondRadius=11.5f;
inline constexpr float InfoY=357;
// Beyond a stroke's radius, where its edge's coverage may still reach.
inline constexpr float Edge=1;
// Room around the date's ink for its edges' coverage.
inline constexpr int DatePad=2;
}
// The centre of the date's ink, on the centre's horizontal. Noonish's is
// further right, as in its reference.
inline constexpr float AnalogDateX=361.5f,NoonishDateX=369;
// What the face shows of a reading. The hands move in steps of ten seconds
// (docs/task11/plan.md 2.1): `step` counts them over twelve hours, so the
// minute hand stands at step%360 degrees and the hour hand at step/12. Being
// integers, a change is compared exactly, and 0:00 is 12:00.
struct AnalogTime {
    bool valid=false;   // invalid: no hands, no dot, the date as --
    int step=0;         // 0..4319
    int second=0;       // 0..59, the dot
    int day=0;          // 1..31
    // What the strokes' pixels depend on besides the layout; 0 when hidden.
    int hourKey() const { return valid ? step+1 : 0; }
    int minuteKey() const { return valid ? step%360+1 : 0; }
    int secondKey() const { return valid ? second+1 : 0; }
};
inline AnalogTime analogTime(const WatchData& d) {
    const auto& t=d.localTime;
    AnalogTime a;
    if (!d.timeValid || t.tm_hour<0 || t.tm_hour>23 || t.tm_min<0 || t.tm_min>59 ||
        t.tm_sec<0 || t.tm_sec>60 || t.tm_mday<1 || t.tm_mday>31) return a;
    // A leap second shows as :59, as the digital faces show it.
    a.second=std::min(t.tm_sec,59);
    a.step=(t.tm_hour%12)*360+t.tm_min*6+a.second/10;
    a.day=t.tm_mday;
    a.valid=true;
    return a;
}
// Clockwise from twelve o'clock.
inline float analogHourDegrees(const AnalogTime& t) { return t.step/12.0f; }
inline float analogMinuteDegrees(const AnalogTime& t) { return float(t.step%360); }
inline float analogSecondDegrees(const AnalogTime& t) { return 6.0f*t.second; }
// The day without a leading zero, or "--".
inline void formatAnalogDate(const AnalogTime& t,char (&out)[3]) {
    if (!t.valid) { out[0]=out[1]='-'; out[2]=0; return; }
    if (t.day<10) { out[0]=char('0'+t.day); out[1]=0; return; }
    out[0]=char('0'+t.day/10); out[1]=char('0'+t.day%10); out[2]=0;
}
struct AnalogPoint { float x=0,y=0; };
// A hand, the hub or the dot as the painter draws it: a line of radius r from
// a to b with round ends, a disc when a is b.
struct AnalogStroke { AnalogPoint a{},b{}; float r=0; };
struct AnalogLayout {
    float scale=1;
    AnalogPoint centre{};
    float hourLength=0,hourRadius=0,minuteLength=0,minuteRadius=0;
    float hubRadius=0,secondOrbit=0,secondRadius=0;
    AnalogPoint date{};     // the centre of the date's ink
    InfoRow row{};
};
// `dateX` is the face's reference position of the date (AnalogDateX,
// NoonishDateX). The circle is centred in the viewport and scaled to its
// shorter side.
inline AnalogLayout analogLayout(const Viewport& v,float dateX) {
    using namespace analog;
    AnalogLayout l;
    l.scale=float(std::min(v.width,v.height))/AnalogReference;
    l.centre={(v.width-1)/2.0f,(v.height-1)/2.0f};
    l.hourLength=HourLength*l.scale; l.hourRadius=HourWidth/2*l.scale;
    l.minuteLength=MinuteLength*l.scale; l.minuteRadius=MinuteWidth/2*l.scale;
    l.hubRadius=HubRadius*l.scale;
    l.secondOrbit=SecondOrbit*l.scale; l.secondRadius=SecondRadius*l.scale;
    l.date={l.centre.x+(dateX-Centre)*l.scale,l.centre.y};
    l.row=infoRow(v,int(std::lround(l.centre.y+(InfoY-Centre)*l.scale)));
    return l;
}
// `length` from the centre towards `degrees`.
inline AnalogPoint analogPoint(const AnalogLayout& l,float length,float degrees) {
    const double a=degrees*3.14159265358979323846/180;
    return {float(l.centre.x+length*std::sin(a)),float(l.centre.y-length*std::cos(a))};
}
inline AnalogStroke analogHour(const AnalogLayout& l,const AnalogTime& t) {
    return {l.centre,analogPoint(l,l.hourLength,analogHourDegrees(t)),l.hourRadius};
}
inline AnalogStroke analogMinute(const AnalogLayout& l,const AnalogTime& t) {
    return {l.centre,analogPoint(l,l.minuteLength,analogMinuteDegrees(t)),l.minuteRadius};
}
inline AnalogStroke analogSecond(const AnalogLayout& l,const AnalogTime& t) {
    const auto p=analogPoint(l,l.secondOrbit,analogSecondDegrees(t));
    return {p,p,l.secondRadius};
}
inline AnalogStroke analogHub(const AnalogLayout& l) { return {l.centre,l.centre,l.hubRadius}; }
// Every pixel a stroke's coverage can reach, rounded outwards: the rectangle
// drawWideLineClipped scans for it.
inline Rect strokeBounds(const AnalogStroke& s) {
    const float reach=s.r+analog::Edge;
    const int x0=int(std::floor(std::min(s.a.x,s.b.x)-reach)),x1=int(std::ceil(std::max(s.a.x,s.b.x)+reach));
    const int y0=int(std::floor(std::min(s.a.y,s.b.y)-reach)),y1=int(std::ceil(std::max(s.a.y,s.b.y)+reach));
    return {x0,y0,x1-x0+1,y1-y0+1};
}
// Where the date goes, from what the painter's font measures of the text: its
// advance and its ink above and below the baseline. The ink is centred on the
// layout's date point; x is its left edge, the baseline for baseline_left,
// and the box holds the ink with room for its edges.
struct AnalogDatePlace { int x=0,baseline=0; Rect box{}; };
inline AnalogDatePlace placeAnalogDate(const AnalogLayout& l,int width,int ascent,int descent) {
    AnalogDatePlace p;
    p.x=int(std::lround(l.date.x-(width-1)/2.0f));
    p.baseline=int(std::lround(l.date.y+(ascent-descent+1)/2.0f));
    const int pad=analog::DatePad;
    p.box={p.x-pad,p.baseline-ascent-pad,width+2*pad,ascent+descent+2*pad};
    return p;
}
// What Analog and Noonish share without their drawing: whether the dot shows,
// the battery rule and the items, and when to draw again. Each face owns one,
// bound to its own record, where the dot is kept as the time variant (with
// seconds: the dot shows). A long press anywhere shows or hides the dot; a tap
// means nothing: the list opens with the swipe up or A/B.
class AnalogControl {
public:
    bool seconds() const { return seconds_; }
    // Once, when the face is registered: the stored choice, or no dot for
    // anything else.
    void bindPreferences(FacePreferences* prefs) {
        record_.bind(prefs);
        seconds_=record_.restore(TimeVariant::HourMinute)==TimeVariant::HourMinuteSecond;
    }
    // The frame about to be drawn. The same input twice leaves the same state.
    void update(const WatchData& d) {
        items_=std::clamp<int>(d.background.count,0,AnalogMaxItems);
        battery_=infoBatteryShown(d.batteryPercent,d.charging,d.chargingKnown,battery_);
    }
    bool batteryShown() const { return battery_; }
    int items() const { return items_; }
    HomeOutcome handle(const HomeEvent& e) {
        HomeOutcome out;
        if (e.kind!=HomeEventKind::LongPress) return out;
        seconds_=!seconds_;
        out.changed=true;
        out.saveFailed=!record_.save(seconds_ ? TimeVariant::HourMinuteSecond : TimeVariant::HourMinute);
        return out;
    }
    // The hands move every ten seconds with or without the dot; the dot every
    // second. Nothing moves while the time is invalid.
    TimeUs nextUpdate(TimeUs now,const WatchData& d) const {
        if (!analogTime(d).valid) return INT64_MAX;
        return seconds_ ? nextSecond(now,d) : nextTenSeconds(now,d);
    }
    // The first two items, in the providers' order, are the ones drawn.
    BackgroundInterest backgroundInterest(const BackgroundSnapshot& s) const { return leadingItems(s,AnalogMaxItems); }
private:
    bool seconds_=false;
    VariantRecord record_;
    bool battery_=false;
    int items_=0;
};
}
