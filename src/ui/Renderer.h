#pragma once
#include "DigitalWatchFace.h"
#include "ExternalLayer.h"
#include "SettingsLayer.h"
#include "StatsOverlay.h"
#include "StopwatchLayer.h"
#include "app/RenderPort.h"
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
    uint32_t layouts() const { return layouts_; }
    uint32_t paints() const { return paints_; }
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    void capacityForTest(int n) { capacity_=n; invalidate(); }
    // The chip carries a clock, so two draws of the same model differ. The
    // pixel comparison turns it off and checks the settings row instead.
    void suppressStatsForTest(bool suppress) { statsSuppressed_=suppress; invalidate(); }
#endif
private:
    struct Row { RowLayout layout{}; char name[96]{}; int handle=-1; };
    void planList(Viewport,ListGeometry,const AppListModel&,bool hidden);
    void paintList(Viewport,const AppListModel&);
    void planToast(Viewport,const char*);
    void paintToast(Viewport,const char*);
    M5GFX& display_;
    DigitalWatchFace digital_;
    SettingsLayer settings_;
    ExternalLayer external_;
    StopwatchLayer stopwatch_;
    StatsOverlay stats_;
    bool statsSuppressed_=false;
    std::array<WatchFace*,4> registry_{};
    WatchFace* face_=nullptr;
    const lgfx::IFont* nameFont_=&fonts::lgfxJapanGothic_24;
    FramePlan frame_;
    std::array<Element,5> rows_{};
    Element toast_;
    std::array<Row,5> plannedRows_{};
    Rect toastBox_{};
    int toastHandle_=-1;
    bool full_=true,overflowReported_=false;
    ScreenId previousScreen_=ScreenId::Home;
    const char* previousToast_=nullptr;
    uint32_t layouts_=0,paints_=0;
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    int capacity_=FramePlan::Capacity;
#endif
};
}
