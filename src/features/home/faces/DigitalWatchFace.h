#pragma once
#include "features/home/WatchFace.h"
#include "features/home/faces/DigitalLayout.h"
namespace launcher {
class DigitalWatchFace final : public WatchFace {
public:
    const char* id() const override { return "digital"; }
    bool begin(Gfx&,bool disableCache=false) override;
    void end() override;
    void plan(FramePlan&,Gfx&,const DrawRegion&,const WatchData&) override;
    void paint(Gfx&,const FramePlan&) override;
    TimeUs nextUpdate(TimeUs now,const WatchData& data) const override { return control_.nextUpdate(now,data); }
    HomeOutcome handle(const HomeEvent& e) override { return control_.handle(e,viewport_); }
    BackgroundInterest backgroundInterest(const BackgroundSnapshot& s) const override { return control_.backgroundInterest(s); }
private:
    void timeFont(Gfx& gfx);
    void paintTime(Gfx& gfx);
    // Sized for the variant it was made for; remade once when that changes.
    void makeCache(Gfx& gfx);
    DigitalControl control_;
    M5Canvas cache_;
    bool cacheReady_=false,cacheAllowed_=false;
    DigitalVariant cacheVariant_=DigitalVariant::HourMinute;
    char cached_[12]{},time_[12]{},date_[32]{},battery_[24]{};
    std::array<Element,5> elements_{};
    std::array<Rect,5> boxes_{};
    std::array<int,5> handles_{};
    Viewport viewport_{};
    Rect clip_{},timeBox_{};
    int offset_=0,cx_=0;
    float scale_=1;
};
}
