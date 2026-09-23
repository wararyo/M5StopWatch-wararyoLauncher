#pragma once
#include "WatchFace.h"
namespace launcher {
class DigitalWatchFace final : public WatchFace {
public:
    const char* id() const override { return "digital"; }
    bool begin(Gfx&,bool disableCache=false) override;
    void end() override;
    void plan(FramePlan&,Gfx&,const DrawRegion&,const WatchData&) override;
    void paint(Gfx&,const FramePlan&) override;
    TimeUs nextUpdate(TimeUs now,const WatchData& data) const override { return nextMinute(now,data); }
private:
    void timeFont(Gfx& gfx);
    void paintTime(Gfx& gfx);
    M5Canvas cache_;
    bool cacheReady_=false;
    char cached_[8]{},time_[8]{},date_[32]{},battery_[24]{};
    std::array<Element,5> elements_{};
    std::array<Rect,5> boxes_{};
    std::array<int,5> handles_{};
    Viewport viewport_{};
    Rect clip_{},timeBox_{};
    int offset_=0,cx_=0;
    float scale_=1;
};
}
