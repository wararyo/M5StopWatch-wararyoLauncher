#pragma once
#include "DigitalWatchFace.h"
#include "ExternalLayer.h"
#include "SettingsLayer.h"
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
#endif
private:
    struct Row { RowLayout layout{}; char name[96]{}; int handle=-1; };
    void planList(const ScreenModel&);
    void paintList(const ScreenModel&);
    void planToast(const ScreenModel&);
    void paintToast(const ScreenModel&);
    M5GFX& display_;
    DigitalWatchFace digital_;
    SettingsLayer settings_;
    ExternalLayer external_;
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
