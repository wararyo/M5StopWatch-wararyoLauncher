#include "HostRenderer.h"
#include "assets/AppIcons.h"
#include "ui/graphics/VlwFont.h"
#include "ui/graphics/WatchFonts.h"
#include <esp_timer.h>
#ifdef LAUNCHER_RENDER_METRICS
#include "host/RenderDiagnostics.h"
#endif
namespace launcher {
bool HostRenderer::begin(bool disableCache,WatchPreferences* store) {
    // The list keeps working on the built-in font if the embedded subset fails.
    if(const auto* embedded=vlwFont()) nameFont_=embedded;
    appList_.begin(nameFont_); settings_.begin(nameFont_); external_.begin(nameFont_);
    stopwatch_.begin(nameFont_); toast_.begin(nameFont_);
    timer_.begin(nameFont_,watchTextFont(),timerDigitGlyphs());
    pedometer_.begin(nameFont_,watchTextFont(),timerDigitGlyphs());
    // M5GFX addresses this panel as 468 rows, but a transfer whose window
    // reaches rows 466 and 467 has its first two rows spoiled on the panel
    // (black lines left by the list's edge, docs/task10/10-5-validation.md).
    // Those rows are outside the round glass, so frames never draw them and
    // no transfer reaches them.
    renderer_.limitTo({0,0,int(display_.width()),std::min(int(display_.height()),PanelRows)});
    return display_.width()>0 && display_.height()>0 && home_.begin(display_,disableCache,store);
}
RenderLayer* HostRenderer::layer(FrameLayer id) {
    switch(id) {
    case FrameLayer::Home: return &home_;
    case FrameLayer::AppList: return &appList_;
    case FrameLayer::Settings: return &settings_;
    case FrameLayer::External: return &external_;
    case FrameLayer::Stopwatch: return &stopwatch_;
    case FrameLayer::Timer: return &timer_;
    case FrameLayer::Pedometer: return &pedometer_;
    case FrameLayer::HomeGesture: return &homeGesture_;
    case FrameLayer::Toast: return &toast_;
    }
    return nullptr;
}
void HostRenderer::draw(const FrameModel& m,const WatchData& watch) {
    const TimeUs start=esp_timer_get_time();
    const auto c=composer_.compose(m);
    if(c.changed) { renderer_.invalidate(); home_.resume(); }
    // Home by the gesture: the clock comes in from the top, each frame
    // drawing only the rows it uncovers, and the screen's pixels under the
    // edge are left alone (docs/task14/plan.md 2.6). The clock is composed at
    // rest, so none of its elements moves as the edge passes them.
    const auto reveal=reveal_.next(m.homeGesture,c.viewport);
    if(reveal.start) { renderer_.invalidate(); home_.resume(); }
    renderer_.confine(reveal.confine);
    if(!reveal.strip.empty()) home_.uncover(reveal.strip);
    // Every layer's input is fixed here, before anything plans, and stays
    // untouched until the frame has been painted.
    home_.prepare(c.home,watch);
    // What the frame lies on: under the clock and the list, the colour the
    // selected face lays the list on (a face shows that colour wherever it
    // does not paint); under another screen, that screen's own.
    const uint16_t listBackground=home_.listBackground();
    uint16_t base=DefaultBase;
    if(c.list) base=listBackground;
    else if(c.settings) base=settings_.background();
    else if(c.external) base=external_.background();
    else if(c.stopwatch) base=stopwatch_.background();
    else if(c.timer) base=timer_.background();
    else if(c.pedometer) base=pedometer_.background();
    renderer_.setBase(base);
    appList_.prepare(c.viewport,m.launcher,c.list,listBackground);
    settings_.prepare(c.viewport,m.settings,c.settings,m.stats);
    external_.prepare(c.viewport,m.external,c.external);
    stopwatch_.prepare(c.viewport,m.stopwatch,c.stopwatch);
    timer_.prepare(c.viewport,m.timer,c.timer);
    pedometer_.prepare(c.viewport,m.pedometer,c.pedometer);
    homeGesture_.prepare(c.viewport,m.homeGesture.band,home_.homeGestureBackground(),
                         home_.homeGestureForeground(),appIcon(IconId::WatchFace));
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
