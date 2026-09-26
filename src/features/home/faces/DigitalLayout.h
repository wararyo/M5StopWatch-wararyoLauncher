#pragma once
#include "features/home/HomeInteraction.h"
#include "ui/rendering/Scale.h"
namespace launcher {
enum class DigitalVariant : uint8_t { HourMinute,HourMinuteSecond };
// Where Digital puts its parts, in the clock's own coordinates (no slide).
// Text is measured by the painter around these centres; the APPS target is
// fixed here, so what is drawn and what a tap hits cannot drift apart.
struct DigitalLayout {
    int cx=0,batteryY=0,dateY=0,timeY=0,appsLabelY=0;
    Rect appsIcon{},apps{};
};
inline DigitalLayout digitalLayout(const Viewport& v) {
    DigitalLayout l;
    l.cx=v.width/2;
    l.batteryY=scaled(v,80); l.dateY=scaled(v,123); l.timeY=scaled(v,250); l.appsLabelY=scaled(v,400);
    l.appsIcon={l.cx-scaled(v,16),scaled(v,357),scaled(v,32),scaled(v,32)};
    // The icon and the label under it, with room for a fingertip.
    l.apps={l.cx-scaled(v,64),scaled(v,351),scaled(v,128),scaled(v,70)};
    return l;
}
// Digital's behaviour without its drawing: the variant, what a tap or a long
// press means, and when the face has to be drawn again. The variant outlives
// the face's caches (begin/end), so it survives leaving and coming back.
class DigitalControl {
public:
    DigitalVariant variant() const { return variant_; }
    HomeOutcome handle(const HomeEvent& e,const Viewport& v) {
        HomeOutcome out;
        if (e.kind==HomeEventKind::LongPress) {
            // Anywhere, APPS included: the press was not a tap.
            variant_=variant_==DigitalVariant::HourMinute ? DigitalVariant::HourMinuteSecond : DigitalVariant::HourMinute;
            out.changed=true;
        } else if (digitalLayout(v).apps.contains(e.x,e.y)) out.request=HomeRequest::OpenAppList;
        return out;
    }
    TimeUs nextUpdate(TimeUs now,const WatchData& d) const {
        return variant_==DigitalVariant::HourMinuteSecond ? nextSecond(now,d) : nextMinute(now,d);
    }
    // Nothing yet: the information row comes with task 10-3, and a label the
    // face does not draw must not wake the display.
    BackgroundInterest backgroundInterest(const BackgroundSnapshot&) const { return {}; }
private:
    DigitalVariant variant_=DigitalVariant::HourMinute;
};
}
