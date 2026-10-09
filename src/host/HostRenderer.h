#pragma once
#include "host/FrameComposer.h"
#include "host/RenderPort.h"
#include "features/external/ExternalLayer.h"
#include "features/home/HomeLayer.h"
#include "features/launcher/AppListLayer.h"
#include "features/settings/SettingsLayer.h"
#include "features/stopwatch/StopwatchLayer.h"
#include "features/timer/TimerLayer.h"
#include "features/pedometer/PedometerLayer.h"
#include "ui/overlays/StatsOverlay.h"
#include "ui/overlays/ToastLayer.h"
#include "ui/rendering/Renderer.h"
namespace launcher {
// The app's drawing: it owns every layer, hands each one its part of the frame
// model, puts them in order and decides when the composition itself calls for
// a full repaint. The shared Renderer then runs the frame without knowing any
// of them. Diagnostics hook in here too, after the Renderer returns.
//
// Static in main: the layers hold framebuffer metadata and caches that must
// stay off the 8KiB UI stack.
class HostRenderer final : public RenderPort, public HomeControlPort {
public:
    explicit HostRenderer(M5GFX& display):display_(display),renderer_(display) {}
    // `store` holds the face selection and the faces' records; null keeps
    // everything in RAM (the render check).
    bool begin(bool disableCache=false,WatchPreferences* store=nullptr);
    bool registerFace(WatchFace& face) { return home_.registerFace(face); }
    bool selectFace(const char* id,bool disableCache=false) { return home_.selectFace(display_,id,disableCache); }
    void invalidate() override { renderer_.invalidate(); home_.resume(); }
    void draw(const FrameModel&,const WatchData&) override;
    TimeUs nextUpdate(TimeUs now,const WatchData& data) const override { return home_.nextUpdate(now,data); }
    BackgroundInterest backgroundInterest(const WatchData& data) const override { return home_.backgroundInterest(data); }
    HomeOutcome handle(const HomeEvent& event) override { return home_.handle(event); }
    int faceCount() const override { return home_.faceCount(); }
    WatchFaceChoice faceAt(int i) const override { return home_.faceAt(i); }
    int currentFace() const override { return home_.currentFace(); }
    // Settings' confirmed choice. The face begins now even behind settings;
    // its first frame is a full repaint whenever it next shows.
    FaceChoiceResult chooseFace(const char* id) override { return home_.chooseFace(display_,id); }
    uint16_t listBackground() const { return home_.listBackground(); }
    const lgfx::IFont* listFont() const { return nameFont_; }
    const ListView& listView() const { return appList_.view(); }
    const ListView& settingsListView() const { return settings_.menuView(); }
    uint32_t layouts() const { return renderer_.layouts(); }
    uint32_t paints() const { return renderer_.paints(); }
    Rect lastDirty() const { return renderer_.lastDirty(); }
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    void capacityForTest(int n) { renderer_.capacityForTest(n); }
    // The chip carries a clock, so two draws of the same model differ. The
    // pixel comparison turns it off and checks the settings row instead.
    void suppressStatsForTest(bool suppress) { statsSuppressed_=suppress; invalidate(); }
    ListView& listViewForTest() { return appList_.view(); }
    SettingsLayer& settingsForTest() { return settings_; }
    int digitalCachedParts() const { return home_.digital().cachedParts(); }
    int forestCachedParts() const { return home_.forest().cachedParts(); }
    bool analogSeconds() const { return home_.analog().seconds(); }
    bool noonishSeconds() const { return home_.noonish().seconds(); }
#endif
private:
    // Rows of the panel a transfer may reach (HostRenderer::begin).
    static constexpr int PanelRows=466;
    RenderLayer* layer(FrameLayer id);
    M5GFX& display_;
    Renderer renderer_;
    const lgfx::IFont* nameFont_=&fonts::lgfxJapanGothic_24;
    HomeLayer home_;
    AppListLayer appList_;
    SettingsLayer settings_;
    ExternalLayer external_;
    StopwatchLayer stopwatch_;
    TimerLayer timer_;
    PedometerLayer pedometer_;
    ToastLayer toast_;
    StatsOverlay stats_;
    FrameComposer composer_;
    bool statsSuppressed_=false;
};
}
