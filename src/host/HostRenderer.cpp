#include "HostRenderer.h"
#include "ui/graphics/VlwFont.h"
#include <esp_timer.h>
#ifdef LAUNCHER_RENDER_METRICS
#include "host/RenderDiagnostics.h"
#endif
namespace launcher {
bool HostRenderer::begin(bool disableCache) {
    // The list keeps working on the built-in font if the embedded subset fails.
    if(const auto* embedded=vlwFont()) nameFont_=embedded;
    appList_.begin(nameFont_); settings_.begin(nameFont_); external_.begin(nameFont_);
    stopwatch_.begin(nameFont_); toast_.begin(nameFont_);
    return display_.width()>0 && display_.height()>0 && home_.begin(display_,disableCache);
}
RenderLayer* HostRenderer::layer(FrameLayer id) {
    switch(id) {
    case FrameLayer::Home: return &home_;
    case FrameLayer::AppList: return &appList_;
    case FrameLayer::Settings: return &settings_;
    case FrameLayer::External: return &external_;
    case FrameLayer::Stopwatch: return &stopwatch_;
    case FrameLayer::Toast: return &toast_;
    }
    return nullptr;
}
void HostRenderer::draw(const FrameModel& m,const WatchData& watch) {
    const TimeUs start=esp_timer_get_time();
    const auto c=composer_.compose(m);
    if(c.changed) { renderer_.invalidate(); home_.resume(); }
    // Every layer's input is fixed here, before anything plans, and stays
    // untouched until the frame has been painted.
    home_.prepare(c.home,watch,m.launcher.transition);
    appList_.prepare(c.viewport,m.launcher,c.list);
    settings_.prepare(c.viewport,m.settings,c.settings,m.stats);
    external_.prepare(c.viewport,m.external,c.external);
    stopwatch_.prepare(c.viewport,m.stopwatch,c.stopwatch);
    toast_.prepare(c.viewport,m.toast);
    RenderLayer* layers[FrameLayerCount];
    for(int i=0;i<FrameLayerCount;++i) layers[i]=layer(FrameOrder[i]);
    // Only when the overlay is on: an unused build pays nothing for it.
    const bool stats=m.stats && !statsSuppressed_;
    const bool painted=renderer_.draw(layers,FrameLayerCount,stats ? &stats_ : nullptr);
    // After endWrite, which is where this panel flushes the modified region
    // over QSPI. The chip and [RenderDiag] measure the same span.
    const TimeUs end=esp_timer_get_time();
    if(painted && stats) stats_.record(start,end);
#ifdef LAUNCHER_RENDER_METRICS
    recordRender(m.activity,start,end,painted);
#endif
}
}
