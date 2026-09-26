#pragma once
#include "features/home/WatchFace.h"
#include "features/home/faces/DigitalLayout.h"
#include "ui/graphics/VlwGlyphs.h"
namespace launcher {
// The default face: battery, date, the time in D-DIN-PRO (hours and minutes,
// or with seconds after a long press), up to two background items as chips,
// and APPS. Positions come from DigitalLayout; this class measures the fonts,
// keeps the caches and paints on black (docs/task10/plan-10-3.md), which is
// the Renderer's base, so it has no background of its own to restore. It
// slides up with the list by itself (docs/task10/plan-10-4.md 5).
class DigitalWatchFace final : public WatchFace {
public:
    const char* id() const override { return "digital"; }
    bool begin(Gfx&,bool disableCache=false) override;
    void end() override;
    void update(const WatchData& d,const WatchEnvironment&,WatchChanges) override { control_.update(d); }
    void plan(FramePlan&,Gfx&,const WatchEnvironment&,const WatchData&) override;
    void paint(Gfx&,const PaintContext&) override;
    TimeUs nextUpdate(TimeUs now,const WatchData& data) const override { return control_.nextUpdate(now,data); }
    HomeOutcome handle(const HomeEvent& e) override { return control_.handle(e,viewport_); }
    BackgroundInterest backgroundInterest(const BackgroundSnapshot& s) const override { return control_.backgroundInterest(s); }
    // Which caches exist, for the diagnostics.
    int cachedParts() const;
private:
    enum Part { Battery,Date,Hour,Minute,Second,Item0,Item1,Apps,PartCount };
    // A font and the text size it is drawn at: 1 for the embedded subsets,
    // more for a built-in fallback.
    struct Font { const lgfx::IFont* font=nullptr; float size=1; int ascent=0,descent=0; };
    // A time group's cache holds what it was drawn with; the same text and
    // variant are never drawn twice, whatever else made the frame dirty.
    struct TimeCache {
        M5Canvas sprite;
        bool ready=false;
        char drawn[12]{};
        DigitalVariant variant=DigitalVariant::HourMinute;
    };
    static constexpr int IconSize=36;
    struct Chip {
        char label[BackgroundLabelBytes+4]{};  // fitted, possibly with "..."
        const IconBitmap* icon=nullptr;
        uint16_t fill=0,ink=0;
        bool wide=false;                        // non-ASCII: the Japanese font
        Rect box{};                             // clock coordinates, no slide
        uint32_t key=0;
        // The icon scaled for the chip, remade when another asset arrives.
        const IconBitmap* scaled=nullptr;
        bool maskReady=false;
        uint8_t mask[IconSize*IconSize]{};
        M5Canvas sprite;
        bool cacheReady=false;
        uint32_t drawnKey=0;
    };
    void useFont(Gfx& g,const Font& f) const;
    // The time from the embedded digits, or the built-in fallback font.
    void drawTime(Gfx& g,const char* text,int x,int y,textdatum_t datum);
    void measure(Gfx& g);
    void makeCaches();
    void paintBattery(Gfx& g,int dx,int dy);
    void paintDate(Gfx& g,int dx,int dy);
    void paintHour(Gfx& g,int dx,int dy);
    void paintMinute(Gfx& g,int dx,int dy);
    void paintSecond(Gfx& g,int dx,int dy);
    void paintChip(Gfx& g,const Chip& p,int dx,int dy);
    void paintApps(Gfx& g,int dx,int dy);
    void paintTime(Gfx& g,TimeCache& cache,const char* text,const Rect& box,void (DigitalWatchFace::*draw)(Gfx&,int,int));
    Rect shifted(Rect r) const { r.y+=offset_; return r; }
    DigitalControl control_;
    // The time's digits are pushed from the asset (VlwGlyphs); time_ is only
    // the built-in font used when that asset cannot be read.
    const VlwGlyphs* timeGlyphs_=nullptr;
    Font time_,text_,small_,wide_;
    DigitalMetrics metrics_{};
    DigitalLayout layout_{};
    DigitalVariant variant_=DigitalVariant::HourMinute;
    bool cacheAllowed_=false;
    TimeCache hourCache_,minuteCache_,secondCache_;
    Chip chips_[DigitalMaxItems];
    int chipCount_=0;
    char hour_[12]{},minute_[12]{},second_[12]{},date_[32]{},battery_[12]{};
    int batteryPercent_=-1;
    bool charging_=false;
    std::array<Element,PartCount> elements_{};
    std::array<Rect,PartCount> boxes_{};
    Viewport viewport_{};
    Rect clip_{};
    int offset_=0;
};
}
