#pragma once
#include "DigitalWatchFace.h"
#include "ExternalLayer.h"
#include "SettingsLayer.h"
#include "StatsOverlay.h"
#include "StopwatchLayer.h"
#include "app/RenderPort.h"
#include "features/launcher/AppListLayer.h"
#include "ui/overlays/ToastLayer.h"
namespace launcher {
class Renderer final : public RenderPort {
public:
    explicit Renderer(M5GFX& display):display_(display) {}
    ~Renderer() override { if(face_) face_->end(); }
    bool begin(bool disableCache=false);
    bool registerFace(WatchFace& face);
    bool selectFace(const char* id,bool disableCache=false);
    void invalidate() override { full_=true; }
    void draw(const ScreenModel&,const WatchData&) override;
    TimeUs nextUpdate(TimeUs now,const WatchData& data) const override { return face_ ? face_->nextUpdate(now,data) : INT64_MAX; }
    const lgfx::IFont* listFont() const { return nameFont_; }
    const ListView& listView() const { return appList_.view(); }
    uint32_t layouts() const { return layouts_; }
    uint32_t paints() const { return paints_; }
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    void capacityForTest(int n) { capacity_=n; invalidate(); }
    // The chip carries a clock, so two draws of the same model differ. The
    // pixel comparison turns it off and checks the settings row instead.
    void suppressStatsForTest(bool suppress) { statsSuppressed_=suppress; invalidate(); }
    ListView& listViewForTest() { return appList_.view(); }
#endif
private:
    M5GFX& display_;
    DigitalWatchFace digital_;
    AppListLayer appList_;
    SettingsLayer settings_;
    ExternalLayer external_;
    StopwatchLayer stopwatch_;
    ToastLayer toast_;
    StatsOverlay stats_;
    bool statsSuppressed_=false;
    std::array<WatchFace*,4> registry_{};
    WatchFace* face_=nullptr;
    const lgfx::IFont* nameFont_=&fonts::lgfxJapanGothic_24;
    FramePlan frame_;
    bool full_=true,overflowReported_=false;
    ScreenId previousScreen_=ScreenId::Home;
    uint32_t layouts_=0,paints_=0;
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    int capacity_=FramePlan::Capacity;
#endif
};
}
