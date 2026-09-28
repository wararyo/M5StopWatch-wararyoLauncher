#pragma once
#include "features/home/faces/TimeGroups.h"
#include "ui/graphics/Gfx.h"
#include "ui/graphics/VlwGlyphs.h"
#include <ctime>
namespace launcher {
// The time as both faces draw it (docs/task10/plan-10-3.md 3-4): the digits
// pushed from the embedded glyph set, or a built-in font when that cannot be
// read; HH, mm and ss in the groups of TimeGroups; each group cached as an
// RGB565 image over the face's own background colour, so the time's edges are
// blended with what really lies under it. A group's image is drawn again only
// when its text, the variant or its colours change.
class TimeDigits {
public:
    enum Group { Hour,Minute,Second,GroupCount };
    // `colonLift` applies to the embedded digits only; a fallback font keeps
    // its own colon.
    void measure(Gfx& g,const VlwGlyphs* glyphs,const lgfx::IFont* fallback,float fallbackSize,int colonLift);
    const TimeMetrics& metrics() const { return metrics_; }
    int colonLift() const { return colonLift_; }
    bool embedded() const { return glyphs_!=nullptr; }
    // Sized once per begin for the larger of the two variants' groups, in
    // internal RAM. A cache that fails stays off until the next begin and that
    // group is drawn directly. Returns the bytes allocated.
    int makeCaches(const TimeGroups& hourMinute,const TimeGroups& withSeconds,bool allowed);
    void release();
    int cached() const;
    // The frame's text: two digits each, "--" for an invalid time, no seconds
    // for hours and minutes. A leap second shows as :59.
    void set(const std::tm& t,bool valid,TimeVariant variant);
    const char* text(Group group) const { return text_[group]; }
    TimeVariant variant() const { return variant_; }
    // One group into its box (already where the face puts it this frame),
    // `baseline` likewise: `fg` over `bg`. The caller has set the clip.
    void paint(Gfx& g,Group group,const TimeGroups& layout,int baseline,const Rect& box,uint16_t fg,uint16_t bg);
private:
    struct Cache {
        M5Canvas sprite;
        bool ready=false;
        char drawn[12]{};
        TimeVariant variant=TimeVariant::HourMinute;
        uint16_t fg=0,bg=0;
    };
    void drawText(Gfx& g,const char* text,int x,int y,textdatum_t datum,uint16_t fg,uint16_t bg);
    void drawGroup(Gfx& g,Group group,const TimeGroups& layout,int baseline,int dx,int dy,uint16_t fg,uint16_t bg);
    const VlwGlyphs* glyphs_=nullptr;
    const lgfx::IFont* fallback_=nullptr;
    float fallbackSize_=1;
    TimeMetrics metrics_{};
    int colonLift_=0;
    Cache caches_[GroupCount];
    char text_[GroupCount][12]{};
    TimeVariant variant_=TimeVariant::HourMinute;
};
}
