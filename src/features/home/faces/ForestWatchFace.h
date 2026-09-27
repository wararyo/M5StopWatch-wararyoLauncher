#pragma once
#include "features/home/WatchFace.h"
#include "features/home/faces/ForestLayout.h"
#include "features/home/faces/TimeDigits.h"
namespace launcher {
// Forest (docs/task10/plan-10-5.md 2): sky, ground and trees redrawn from
// shapes inside whatever the frame restores, the time in the sky, and an
// information row on the ground with the battery (below 30% or charging) and
// up to two background items, their icons in white. With either, the scenery
// moves up to make room; that move is declared as damage, and a changing digit
// or label repaints its own box only. It stays still while the list covers it.
//
// The time sits on plain sky and the information row on plain ground in both
// layouts (checked in the host tests), so both are drawn opaque over those
// colours: the time from the shared caches, the row directly.
class ForestWatchFace final : public WatchFace {
public:
    const char* id() const override { return "forest"; }
    const char* name() const override { return "Forest"; }
    const char* storageKey() const override { return "wf_forest"; }
    void bindPreferences(FacePreferences* prefs) override { control_.bindPreferences(prefs); }
    bool begin(Gfx&,bool disableCache=false) override;
    void end() override;
    void update(const WatchData& d,const WatchEnvironment&,WatchChanges) override { control_.update(d); }
    void plan(FramePlan&,Gfx&,const WatchEnvironment&,const WatchData&) override;
    void paint(Gfx&,const PaintContext&) override;
    // Sky and ground cover all of the face that shows.
    Rect opaqueArea() const override { return clip_; }
    TimeUs nextUpdate(TimeUs now,const WatchData& data) const override { return control_.nextUpdate(now,data); }
    HomeOutcome handle(const HomeEvent& e) override { return control_.handle(e); }
    BackgroundInterest backgroundInterest(const BackgroundSnapshot& s) const override { return control_.backgroundInterest(s); }
    TimeVariant variant() const { return control_.variant(); }
    const ForestLayout& layout() const { return layout_; }
    int cachedParts() const { return digits_.cached(); }
private:
    enum Part { Hour,Minute,Second,Battery,Item0,Item1,PartCount };
    struct Font { const lgfx::IFont* font=nullptr; int ascent=0,descent=0; };
    static constexpr int MaxIcon=40;
    // One group of the information row: the battery, or an item.
    struct Group {
        char label[BackgroundLabelBytes+4]{};  // fitted, possibly with "..."
        bool wide=false;                        // non-ASCII: the Japanese font
        const IconBitmap* icon=nullptr;
        const IconBitmap* scaled=nullptr;       // the icon the mask was made from
        bool maskReady=false;
        uint8_t mask[MaxIcon*MaxIcon]{};
    };
    void useFont(Gfx& g,const Font& f) const { g.setFont(f.font); g.setTextSize(1); }
    void paintScenery(Gfx& g,const PaintContext& context);
    void paintBattery(Gfx& g,const Rect& box);
    void paintItem(Gfx& g,const Group& item,const Rect& box);
    ForestControl control_;
    TimeDigits digits_;
    Font small_,wide_;
    ForestMetrics metrics_{};
    ForestLayout layout_{};
    bool cacheAllowed_=false;
    Group groups_[ForestMaxItems];
    int batteryPercent_=-1;
    bool charging_=false,batteryShown_=false;
    char battery_[12]{};
    std::array<Element,PartCount> elements_{};
    std::array<Rect,PartCount> boxes_{};
    Viewport viewport_{};
    Rect clip_{},shownClip_{};
    bool planned_=false,shownInfo_=false;
};
}
