#include "RenderDiagnostics.h"
#ifdef LAUNCHER_RENDER_METRICS
#include "host/HostRenderer.h"
#include "storage/Settings.h"
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include <cstdio>
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
#include "assets/AppIcons.h"
#include "ui/graphics/Text.h"
#include "ui/graphics/VlwFont.h"
#include "ui/graphics/WatchFonts.h"
#include "ui/graphics/Shapes.h"
#include "features/launcher/LauncherController.h"
#include "host/LaunchRegistry.h"
#include "i18n/Strings.h"
#include <cstring>
#endif
namespace launcher {
namespace {
// Metrics alone have no repaint check to wait for.
bool recording=true;
struct Distribution {
    uint32_t count=0,maximum=0,over=0;
    uint64_t total=0;
    uint32_t buckets[256]{}; // 1ms bins; last bin reports >255ms
    void add(TimeUs us) {
        auto value=static_cast<uint32_t>(std::min<TimeUs>(us,UINT32_MAX));
        ++count; total+=value; maximum=std::max(maximum,value); if(value>33333) ++over;
        ++buckets[std::min<uint32_t>(value/1000,255)];
    }
    void print(const char* name) const {
        uint32_t accumulated=0,p95=0;
        for(uint32_t i=0;i<256;++i) { accumulated+=buckets[i]; if(accumulated*100ULL>=count*95ULL) { p95=i; break; } }
        std::printf("[RenderDiag] %s n=%u avg=%llu max=%u p95_upper=%s%u us over33333=%u\n",
            name,unsigned(count),count ? total/count : 0,unsigned(maximum),p95==255 ? ">" : "",
            unsigned((p95==255 ? 255 : p95+1)*1000),unsigned(over));
    }
};
// A frame is classified by the activity the app composed into its model, so
// one window can mix scenarios and still report each of them separately.
// `scroll` keeps its name for the launcher so earlier records stay comparable.
enum Mode { ModeTransition, ModeScroll, ModeSettingsScroll, ModeStopwatch,
            ModeSettingsSingle, ModeSingle, ModeCount };
constexpr int IntervalModes=ModeSettingsSingle; // Continuous by nature; single frames are not.
Mode modeOf(FrameActivity activity) {
    switch(activity) {
    case FrameActivity::Transition: return ModeTransition;
    case FrameActivity::LauncherScroll: return ModeScroll;
    case FrameActivity::SettingsScroll: return ModeSettingsScroll;
    case FrameActivity::Stopwatch: return ModeStopwatch;
    case FrameActivity::SettingsSingle: return ModeSettingsSingle;
    default: return ModeSingle;
    }
}
// An interval only describes a stretch where the operation keeps producing
// frames. A longer gap is a pause - a stopped finger, a screen with nothing to
// update - and goes to a counter instead of the continuous figures, so it is
// out of the steady-stretch judgement of plan.md 6.3 without being silently
// dropped: the unfiltered distribution is printed next to it.
constexpr TimeUs ContinuousGapUs=200000;
Distribution drawTime[ModeCount],interval[IntervalModes],continuous[IntervalModes],inputLatency,wakeLatency;
uint32_t pauses[IntervalModes]{};
TimeUs longestPause[IntervalModes]{};
TimeUs lastEnd=0,windowStart=0,inputAt=-1,wakeAt=-1;
int lastMode=-1;
uint32_t lastLayouts=0,lastPaints=0,loops=0;
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
WatchData sampleData() {
    WatchData d; d.timeValid=true; d.localTime.tm_hour=9; d.localTime.tm_min=41;
    d.localTime.tm_mon=8; d.localTime.tm_mday=19; d.localTime.tm_wday=6;
    d.batteryPercent=82; return d;
}
#ifdef LAUNCHER_RENDER_SHOTS
// One frame as text: "[Shot] name w h", then base64 lines of run-length
// pairs (run-1, then the pixel as read back, high byte first), then
// "[ShotEnd]". The panel is mostly black, so runs keep it small.
void dumpShot(const char* name,const uint16_t* pixels,int w,int h) {
    static const char digits[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint8_t chunk[57]; int used=0; unsigned total=0;
    auto flush=[&] {
        if(!used) return;
        char line[80]; int n=0;
        for(int i=0;i<used;i+=3) {
            const uint32_t v=uint32_t(chunk[i])<<16|(i+1<used ? uint32_t(chunk[i+1])<<8 : 0)|(i+2<used ? chunk[i+2] : 0);
            line[n++]=digits[v>>18&63]; line[n++]=digits[v>>12&63];
            line[n++]=i+1<used ? digits[v>>6&63] : '='; line[n++]=i+2<used ? digits[v&63] : '=';
        }
        line[n]=0; std::printf("[ShotData] %s\n",line); total+=used; used=0;
    };
    auto put=[&](uint8_t b) { chunk[used++]=b; if(used==57) flush(); };
    std::printf("[Shot] %s %d %d\n",name,w,h);
    const size_t count=size_t(w)*h;
    for(size_t i=0,runs=0;i<count;++runs) {
        size_t run=1;
        while(run<256 && i+run<count && pixels[i+run]==pixels[i]) ++run;
        put(uint8_t(run-1)); put(uint8_t(pixels[i]>>8)); put(uint8_t(pixels[i]));
        i+=run;
        if(runs%2048==2047) vTaskDelay(1);
    }
    flush();
    std::printf("[ShotEnd] %s bytes=%u\n",name,total);
}
#endif
class TestFace final : public WatchFace {
    static constexpr Rect A{40,100,170,100},B{130,120,170,100};
    Element a_,b_;
public:
    const char* id() const override { return "test-overlap"; }
    bool begin(Gfx&,bool) override { a_={}; b_={}; return true; }
    void end() override {}
    void plan(FramePlan& f,Gfx&,const WatchEnvironment& env,const WatchData& d) override {
        f.add(a_,intersect(A,env.clip),hashValue(d.localTime.tm_min));
        f.add(b_,intersect(B,env.clip),1);
    }
    void paint(Gfx& g,const PaintContext& c) override {
        if(c.clip(g,A)) g.fillRect(A.x,A.y,A.w,A.h,0x1234);
        if(c.clip(g,B)) g.fillRect(B.x,B.y,B.w,B.h,0x5678);
    }
    TimeUs nextUpdate(TimeUs,const WatchData&) const override { return INT64_MAX; }
};
// A face with scenery, for checking the damage against a full repaint on
// something other than black (docs/task10/plan-10-4.md 6-7). Sky bands, a
// sun and a tree with antialiased edges, and ground; with any background item
// the scenery moves up to make room, as Forest will. Over it the time and an
// item chip are drawn transparently, blending with the restored scenery, and
// a ring overlaps the time without ever changing. It stays still while the
// list covers it, and asks for a list background of any colour.
class BackdropFace final : public WatchFace {
    enum Part { Time,Ring,Chip,PartCount };
    Element elements_[PartCount];
    Rect boxes_[PartCount]{};
    WatchEnvironment env_{};
    Rect shownClip_{};
    bool info_=false,shownInfo_=false,planned_=false;
    char time_[8]{},label_[BackgroundLabelBytes]{};
    uint16_t list_=0x0000;
    const lgfx::IFont* font() const { return watchTextFont() ? watchTextFont() : &fonts::FreeSans18pt7b; }
    // The ground's top edge: the scenery's one moving part.
    int horizon() const { return info_ ? 250 : 300; }
public:
    const char* id() const override { return "test-backdrop"; }
    void listBackgroundForTest(uint16_t color) { list_=color; }
    uint16_t listBackground(const WatchData&) const override { return list_; }
    bool begin(Gfx&,bool) override { for(auto& e:elements_) e={}; planned_=false; return true; }
    void end() override {}
    void plan(FramePlan& f,Gfx& g,const WatchEnvironment& env,const WatchData& d) override {
        env_=env;
        info_=d.background.count>0;
        // The scenery rearranged: everything of it that shows changes. The
        // list's edge moved: the band between the two bottoms of the clip.
        if(planned_ && info_!=shownInfo_) f.damage(unite(env.clip,shownClip_));
        else if(planned_ && env.clip!=shownClip_) {
            const int a=shownClip_.y+shownClip_.h,b=env.clip.y+env.clip.h;
            f.damage({0,std::min(a,b),env.viewport.width,std::abs(a-b)});
        }
        shownInfo_=info_; shownClip_=env.clip; planned_=true;
        if(d.timeValid) std::snprintf(time_,sizeof(time_),"%02d:%02d",d.localTime.tm_hour%100,d.localTime.tm_min%100);
        else std::strcpy(time_,"--:--");
        std::snprintf(label_,sizeof(label_),"%s",info_ ? d.background.items[0].label : "");
        g.setFont(font()); g.setTextSize(2);
        const int cx=env.viewport.width/2,tw=g.textWidth(time_),th=g.fontHeight();
        boxes_[Time]={cx-tw/2-2,horizon()-40-th,tw+4,th+4};
        boxes_[Ring]={cx+tw/2-30,horizon()-40-th-20,52,52};
        g.setTextSize(1);
        const int lw=info_ ? g.textWidth(label_) : 0;
        boxes_[Chip]=info_ ? Rect{cx-lw/2-24,horizon()+40,lw+48,44} : Rect{};
        g.setTextSize(1);
        const uint32_t hashes[PartCount]={hashString(time_),1,hashString(label_)};
        for(int i=0;i<PartCount;++i) f.add(elements_[i],intersect(boxes_[i],env.clip),hashes[i]);
    }
    void paint(Gfx& g,const PaintContext& c) override {
        const Viewport v=env_.viewport;
        const int h=horizon();
        if(c.clip(g,env_.clip)) {
            constexpr uint16_t Sky[]={0x1a6f,0x2b31,0x4c13,0x7d55};
            const int band=(h+3)/4;
            for(int i=0;i<4;++i) g.fillRect(0,i*band,v.width,band,Sky[i]);
            g.fillSmoothCircle(340,h-150,46,0xfec8);
            g.fillRect(0,h,v.width,v.height-h,0x3a84);
            drawWideLineClipped(g,120,h-170,70,h+2,9.5f,0x1d05);
            drawWideLineClipped(g,120,h-170,170,h+2,9.5f,0x1d05);
            g.fillSmoothCircle(120,h-110,38,0x2ea6);
        }
        if(c.clip(g,boxes_[Time])) {
            g.setFont(font()); g.setTextSize(2); g.setTextDatum(top_center);
            g.setTextColor(0xffff);
            g.drawString(time_,v.width/2,boxes_[Time].y+2);
        }
        if(c.clip(g,boxes_[Ring])) {
            const Rect r=boxes_[Ring];
            g.fillSmoothCircle(r.x+r.w/2,r.y+r.h/2,24,0xf800);
            g.fillSmoothCircle(r.x+r.w/2,r.y+r.h/2,16,0xffe0);
        }
        if(c.clip(g,boxes_[Chip])) {
            const Rect r=boxes_[Chip];
            g.fillSmoothRoundRect(r.x,r.y,r.w,r.h,r.h/2,0x5d1f);
            g.setFont(font()); g.setTextSize(1); g.setTextDatum(middle_center);
            g.setTextColor(0x0000);
            g.drawString(label_,r.x+r.w/2,r.y+r.h/2);
        }
        g.setTextSize(1);
    }
    TimeUs nextUpdate(TimeUs,const WatchData&) const override { return INT64_MAX; }
};
#endif
}
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
WatchData DiagnosticDataSource::sample(TimeUs now) {
    auto d=sampleData();
    const auto seconds=now/1000000;
    // Deterministic sample starting at 09:41 JST; advance date without RTC.
    // Synthetic civil-time timeline decoded in UTC, without changing TZ.
    std::time_t sample=1789810860LL+seconds;
    gmtime_r(&sample,&d.localTime);
    d.subsecondUs=now%1000000;
    return d;
}
#endif
void recordInput(TimeUs now) { if(recording && inputAt<0) inputAt=now; }
void recordWake(TimeUs now) { if(recording) wakeAt=now; }
void recordLoop() { ++loops; }
void recordRender(FrameActivity activity,TimeUs start,TimeUs end,bool painted) {
    if(!recording) return;
    if(!painted) { inputAt=-1; return; }
    const int mode=modeOf(activity);
    drawTime[mode].add(end-start);
    if(mode<IntervalModes && lastMode==mode && lastEnd) {
        const TimeUs gap=end-lastEnd;
        interval[mode].add(gap);
        if(gap<=ContinuousGapUs) continuous[mode].add(gap);
        else { ++pauses[mode]; longestPause[mode]=std::max(longestPause[mode],gap); }
    }
    lastMode=mode; lastEnd=end;
    if(inputAt>=0) { inputLatency.add(end-inputAt); inputAt=-1; }
    if(wakeAt>=0) { wakeLatency.add(end-wakeAt); wakeAt=-1; }
}
void reportRenderDiagnostics(const HostRenderer& renderer,TimeUs now) {
    if(!recording) return;
    if(!windowStart) { windowStart=now; lastLayouts=renderer.layouts(); lastPaints=renderer.paints(); loops=0; return; }
    if(now-windowStart<60000000) return;
    const char* drawNames[]={"transition-draw","scroll-draw","settings-scroll-draw","stopwatch-draw",
                             "settings-single-draw","single-draw"};
    const char* gapNames[]={"transition-interval","scroll-interval","settings-scroll-interval",
                            "stopwatch-interval"};
    for(int i=0;i<ModeCount;++i) drawTime[i].print(drawNames[i]);
    for(int i=0;i<IntervalModes;++i) {
        interval[i].print(gapNames[i]);
        char continuousName[40];
        std::snprintf(continuousName,sizeof(continuousName),"%s-cont",gapNames[i]);
        continuous[i].print(continuousName);
        std::printf("[RenderDiag] %s fps=%.2f cont_fps=%.2f pauses=%u longest_pause=%llu us\n",gapNames[i],
            interval[i].total ? interval[i].count*1000000.0/interval[i].total : 0.0,
            continuous[i].total ? continuous[i].count*1000000.0/continuous[i].total : 0.0,
            unsigned(pauses[i]),static_cast<unsigned long long>(longestPause[i]));
    }
    inputLatency.print("sampled-input-to-end"); wakeLatency.print("wake-request-to-end");
    std::printf("[RenderDiag] window_us=%lld loops=%u layouts=%u paints=%u stack_free=%u internal_free=%u internal_largest=%u psram_free=%u psram_largest=%u\n",
        static_cast<long long>(now-windowStart),unsigned(loops),unsigned(renderer.layouts()-lastLayouts),unsigned(renderer.paints()-lastPaints),
        unsigned(uxTaskGetStackHighWaterMark(nullptr)),unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
        unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
        unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
    // Cumulative since boot. Text images live in PSRAM, one per display slot
    // of each list; the two lists never share them.
    auto printCache=[](const char* name,const ListView& view) {
        const auto& cache=view.cacheStats();
        std::printf("[RenderDiag] %s bytes=%u allocations=%u failures=%u fits=%u renders=%u\n",name,
            unsigned(cache.bytes),unsigned(cache.allocations),unsigned(cache.failures),
            unsigned(cache.fits),unsigned(cache.renders));
    };
    printCache("list_cache",renderer.listView());
    printCache("settings_list_cache",renderer.settingsListView());
    for(auto& d:drawTime) d={};
    for(auto& d:interval) d={};
    for(auto& d:continuous) d={};
    for(auto& p:pauses) p=0;
    for(auto& p:longestPause) p=0;
    inputLatency={}; wakeLatency={}; lastMode=-1; lastEnd=0;
    windowStart=now; lastLayouts=renderer.layouts(); lastPaints=renderer.paints(); loops=0;
}
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
void runRepaintCheck(HostRenderer& renderer,M5GFX& display,const SlotCatalog& catalog) {
    recording=false;
    // The statistics chip carries a clock, so the two draws a comparison makes
    // would differ by whatever the window did between them. It is outside the
    // frame plan and has nothing to verify differentially; the settings row
    // that turns it on is checked below like any other element.
    renderer.suppressStatsForTest(true);
    const int w=display.width(),h=display.height();
    const size_t pixels=static_cast<size_t>(w)*h,bytes=pixels*2;
    auto* incremental=static_cast<uint16_t*>(heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM));
    auto* reference=static_cast<uint16_t*>(heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM));
    unsigned checks=0,failures=0;
    if(!incremental || !reference) {
        std::printf("[Verify] UNVERIFIED: comparison allocation failed\n");
    } else {
        // Explicit known-pattern roundtrip guards against a no-op readRect.
        std::memset(reference,0,bytes); std::memset(incremental,0xff,bytes);
        display.fillScreen(0x1234); display.readRect(0,0,w,h,reference);
        const uint16_t probe=reference[pixels/2];
        display.fillScreen(0); display.readRect(0,0,w,h,incremental);
        const bool readable=probe!=0 && incremental[pixels/2]==0;
        if(!readable) std::printf("[Verify] UNVERIFIED: framebuffer readback probe failed\n");
        else {
            ++checks;
            if(!vlwFont()) { ++failures; std::printf("[Verify] FAIL embedded VLW font not loaded\n"); }
            display.setFont(renderer.listFont()); display.setTextSize(1);
            auto covered=[&](const char* text) {
                char fitted[128]; fitText(display,text,fitted,sizeof(fitted),4096);
                ++checks;
                if(std::strcmp(text,fitted)!=0) { ++failures; std::printf("[Verify] FAIL missing fixed UI glyph: %s\n",text); }
            };
            for(const auto& entry:LaunchRegistry) covered(entry.name);
            // Every fixed string the launcher can put on screen, so a font
            // subset that missed one fails here rather than on the device.
            for(const char* fixed:text::All) covered(fixed);
            // The elapsed time sits in a fixed box and is drawn as one string,
            // so digits of unequal width would shuffle it sideways as it counts.
            {
                display.setFont(&fonts::FreeSansBold24pt7b); display.setTextSize(1);
                const int zero=display.textWidth("0");
                for(char c='1';c<='9';++c) {
                    const char digit[2]={c,0};
                    ++checks;
                    if(display.textWidth(digit)!=zero) {
                        ++failures;
                        std::printf("[Verify] FAIL elapsed digit %c is %d wide, not %d\n",
                            c,int(display.textWidth(digit)),int(zero));
                    }
                }
                // Back to the list font: the checks below measure Japanese
                // with it, and a GFX font would call every glyph missing.
                display.setFont(renderer.listFont()); display.setTextSize(1);
            }
            char fitted[128];
            char small[2]; fitText(display,"a",small,sizeof(small),4096); ++checks;
            if(std::strcmp(small,"a")!=0) { ++failures; std::printf("[Verify] FAIL bounded text capacity\n"); }
            fitText(display,"外部アプリ😀",fitted,sizeof(fitted),4096); ++checks;
            if(std::strcmp(fitted,"外部アプリ?")!=0) { ++failures; std::printf("[Verify] FAIL unsupported glyph fallback\n"); }
            // Every row needs its mask, and the mask must stay inside the
            // smaller unselected circle: pushGrayscaleImage paints the whole
            // rectangle, so an overhang would show as a square corner.
            {
                const Viewport probe{w,h};
                const float iconScale=float(std::min(w,h))/468;
                const int radius=iconRadius(probe)-selectionGrowth(probe);
                for(const auto& entry:LaunchRegistry) {
                    const auto* icon=appIcon(entry.icon); ++checks;
                    if(!icon) { ++failures; std::printf("[Verify] FAIL missing icon: %s\n",entry.name); continue; }
                    const float hx=icon->width*iconScale*0.5f,hy=icon->height*iconScale*0.5f;
                    if(hx*hx+hy*hy>float(radius)*radius) {
                        ++failures;
                        std::printf("[Verify] FAIL icon %dx%d does not fit radius %d: %s\n",
                            icon->width,icon->height,radius,entry.name);
                    }
                }
            }
            auto compare=[&](const char* name) {
                ++checks;
                if(std::memcmp(incremental,reference,bytes)!=0) {
                    ++failures; size_t count=0; int x0=w,y0=h,x1=0,y1=0;
                    for(size_t i=0;i<pixels;++i) if(incremental[i]!=reference[i]) {
                        ++count; int x=i%w,y=i/w;
                        x0=std::min(x0,x); y0=std::min(y0,y); x1=std::max(x1,x); y1=std::max(y1,y);
                    }
                    std::printf("[Verify] FAIL %s pixels=%u box=%d,%d-%d,%d\n",name,unsigned(count),x0,y0,x1,y1);
                }
            };
            // The renderer composes every region from the model itself, so a
            // case only ever sets state; nothing here can leave one stale.
            auto check=[&](const char* name,const FrameModel& m,const WatchData& d) {
                renderer.draw(m,d); display.readRect(0,0,w,h,incremental);
                renderer.invalidate(); renderer.draw(m,d); display.readRect(0,0,w,h,reference);
                compare(name);
            };
            // The list's cached name images against the same names drawn as
            // glyphs, both on a full repaint: the cache has to be invisible.
            auto& view=renderer.listViewForTest();
            auto& settingsLayer=renderer.settingsForTest();
            auto directWith=[&](ListView& list,const char* name,const FrameModel& m,const WatchData& d) {
                renderer.invalidate(); renderer.draw(m,d); display.readRect(0,0,w,h,incremental);
                list.textImagesForTest(false);
                renderer.invalidate(); renderer.draw(m,d); display.readRect(0,0,w,h,reference);
                list.textImagesForTest(true);
                compare(name);
            };
            auto direct=[&](const char* name,const FrameModel& m,const WatchData& d) { directWith(view,name,m,d); };
            FrameModel m; m.viewport={w,h}; auto d=sampleData();
            renderer.invalidate();
            // Preserve the original 24 sweeps, expanded to all five rows.
            for(int variant=0;variant<24;++variant) {
                m.launcher.transition=float(variant%21)/20;
                for(int i=0;i<=17+variant*5;++i) {
                    m.launcher.list.scroll=float((i*3)%(4*rowSpacing(m.viewport)+1)); m.launcher.list.selection=(i/11)%5;
                    renderer.draw(m,d);
                    if(i%8==0) vTaskDelay(1);
                }
                check("original-24",m,d);
            }
            for(int direction: {1,-1}) for(int i=0;i<=20;++i) {
                m.launcher.transition=float(direction==1 ? i : 20-i)/20;
                check("roundtrip",m,d); vTaskDelay(1);
            }
            m.screen=ScreenId::AppList; m.launcher.transition=1;
            for(int i=0;i<5;++i) {
                m.launcher.list.selection=i; m.launcher.list.scroll=i*rowSpacing(m.viewport);
                check("five-rows",m,d);
                m.toast="準備中"; check("toast-on",m,d);
                m.launcher.list.scroll+=8; check("toast-overlap",m,d);
                m.toast=nullptr; check("toast-off",m,d);
            }
            m.launcher.names[2]="非常に長い外部アプリ名と未収録文字😀";
            m.launcher.list.scroll=2*rowSpacing(m.viewport); check("long-japanese",m,d);
            // Settings covers the list rather than sliding it away, so the rows
            // it hides have to be erased by the same differential plan.
            m.launcher.names[2]=nullptr; m.screen=ScreenId::Settings; m.settings=SettingsModel{};
            m.settings.savedBrightness=Settings{}.brightness;
            m.settings.savedScreenOffSec=Settings{}.screenOffSec;
            m.settings.lines[0]="wararyoLauncher";
            m.settings.lines[1]="0.0.0-verify"; m.settings.lines[2]="5.5.0";
            // The menu is the shared list without icons (docs/task9/plan-9-3.md):
            // every row selected where A leaves it, then scroll positions
            // between rows, as a drag or the inertia leaves them.
            const int spacing=rowSpacing(m.viewport);
            for(int row=0;row<SettingsMenuCount;++row) {
                m.settings.menu.selection=row; m.settings.menu.scroll=float(row*spacing);
                check("settings-menu",m,d);
                directWith(settingsLayer.menuViewForTest(),"settings-menu-image",m,d);
            }
            m.settings.menu.selection=2;
            for(int y=0;y<=(SettingsMenuCount-1)*spacing;y+=29) {
                m.settings.menu.scroll=float(y); check("settings-menu-scroll",m,d);
                if(y%87==0) vTaskDelay(1);
            }
            // Its notice, a name that has to be shortened, and an empty one.
            m.settings.menu.scroll=float(2*spacing);
            m.toast="保存しました"; check("settings-menu-toast-on",m,d);
            m.settings.menu.scroll+=8; check("settings-menu-toast-overlap",m,d);
            m.toast=nullptr; check("settings-menu-toast-off",m,d);
            m.settings.menu.selection=0; m.settings.menu.scroll=0;
            settingsLayer.menuLabelForTest("非常に長い設定項目の名前と未収録文字😀を含む行");
            check("settings-menu-long",m,d);
            directWith(settingsLayer.menuViewForTest(),"settings-menu-image-long",m,d);
            settingsLayer.menuLabelForTest(""); check("settings-menu-empty",m,d);
            settingsLayer.menuLabelForTest(nullptr); check("settings-menu-restored",m,d);
            // Menu to an editor and back from a position between rows: the
            // switch keeps the screen id, so only the layer's own full repaint
            // stands between these frames and the other half's leftovers.
            m.settings.menu.selection=3; m.settings.menu.scroll=float(3*spacing-37);
            check("settings-menu-mid",m,d);
            for(const auto view:{SettingsView::Brightness,SettingsView::Info}) {
                m.settings.view=view; m.settings.cursor=0; m.settings.fields[0]=90;
                check("settings-menu-to-view",m,d);
                m.settings.view=SettingsView::Menu; check("settings-view-to-menu",m,d);
            }
            // Leaving from there and entering again, at the top as a new
            // visit starts, and once more where the last one was.
            m.screen=ScreenId::AppList; check("settings-menu-left",m,d);
            m.screen=ScreenId::Settings; check("settings-menu-return",m,d);
            m.settings.menu=ListState{}; m.settings.menu.selection=0; check("settings-menu-reentry",m,d);
            for(const auto view:{SettingsView::DateTime,SettingsView::Brightness,
                                 SettingsView::ScreenOff,SettingsView::Info}) {
                m.settings.view=view;
                m.settings.fields[0]=view==SettingsView::DateTime ? 2026 :
                    view==SettingsView::Brightness ? 90 : 1;
                m.settings.fields[1]=9; m.settings.fields[2]=21;
                m.settings.fields[3]=23; m.settings.fields[4]=59;
                for(int slot=0;slot<settingsSlotCount(view);++slot) {
                    m.settings.cursor=slot; check("settings-slot",m,d);
                }
                // Information carries the statistics action: its label does not
                // change when taken, only its colour, so both states are swept.
                if (settingsActionCount(view)>0) {
                    m.settings.cursor=settingsFieldCount(view);
                    check("settings-action-free",m,d);
                    m.stats=true; check("settings-action-taken",m,d);
                    m.settings.cursor=settingsSlotCount(view)-1;
                    check("settings-action-taken-elsewhere",m,d);
                    m.stats=false;
                }
                m.toast="保存しました"; check("settings-notice",m,d);
                // The longest notice, over the buttons it used to be buried under.
                m.toast="日付が正しくありません"; check("settings-notice-long",m,d);
                m.toast=nullptr; check("settings-notice-off",m,d);
                vTaskDelay(1);
            }
            // The widest date the editor can show, separators included.
            m.settings.view=SettingsView::DateTime; m.settings.cursor=0;
            m.settings.fields[0]=2099; check("settings-widest",m,d);
            m.screen=ScreenId::AppList; m.settings=SettingsModel{};
            check("settings-left",m,d); direct("list-image-reentry",m,d);
            // A row that only changed colour still has to repaint, so the dim
            // flag has to reach the fingerprint.
            m.launcher.list.scroll=2*rowSpacing(m.viewport); m.launcher.list.selection=2;
            for(int i=2;i<5;++i) m.launcher.rowDimmed[i]=true;
            check("list-dimmed",m,d);
            m.launcher.names[2]=catalog.slots[0].name; m.launcher.rowDimmed[2]=false;
            check("list-named",m,d);
            // The shared list's caches (docs/task9/plan-9-2.md 9-2e).
            for(int i=0;i<5;++i) {
                m.launcher.list.selection=i; m.launcher.list.scroll=i*rowSpacing(m.viewport);
                direct("list-image",m,d);
            }
            direct("list-image-dimmed-named",m,d);
            m.launcher.names[2]="非常に長い外部アプリ名と未収録文字😀"; direct("list-image-long",m,d);
            m.launcher.names[2]=""; check("list-empty-name",m,d); direct("list-image-empty",m,d);
            // New content under the same id, then the same name under two ids.
            m.launcher.names[2]="外部アプリA"; check("list-content",m,d);
            m.launcher.names[2]="外部アプリB"; check("list-content-change",m,d); direct("list-image-content",m,d);
            m.launcher.names[2]=m.launcher.names[3]="同じ名前"; m.launcher.rowDimmed[3]=false;
            check("list-same-name",m,d); direct("list-image-same-name",m,d);
            m.launcher.names[3]=nullptr; m.launcher.rowDimmed[3]=true;
            // Moving rows must not shorten or render their names again: one
            // sweep to warm every slot, then a second that may not add any.
            {
                FrameModel sweep=m; sweep.launcher.list.selection=2;
                for(int pass=0;pass<2;++pass) {
                    const auto before=view.cacheStats();
                    for(int y=0;y<=4*rowSpacing(sweep.viewport);y+=6) {
                        sweep.launcher.list.scroll=float(y); renderer.draw(sweep,d);
                        if(y%60==0) vTaskDelay(1);
                    }
                    const auto after=view.cacheStats();
                    if(pass==1) {
                        ++checks;
                        if(after.fits!=before.fits || after.renders!=before.renders || after.allocations!=before.allocations) {
                            ++failures;
                            std::printf("[Verify] FAIL list cache rebuilt on scroll: fits+%u renders+%u allocations+%u\n",
                                unsigned(after.fits-before.fits),unsigned(after.renders-before.renders),
                                unsigned(after.allocations-before.allocations));
                        }
                    }
                }
                check("list-scroll-cached",sweep,d);
            }
            // Slots handed from row to row, and more visible rows than slots:
            // three slots, over transitions that show from one row to six.
            view.slotLimitForTest(3);
            for(int step=1;step<=20;++step) {
                m.launcher.transition=float(step)/20;
                for(int y: {0,50,2*rowSpacing(m.viewport),4*rowSpacing(m.viewport)}) {
                    m.launcher.list.scroll=float(y); check("list-slot-reuse",m,d);
                }
                vTaskDelay(1);
            }
            view.slotLimitForTest(ListVisibleSlots); check("list-slot-restored",m,d);
            // An image that cannot be allocated draws the name directly, and
            // the same key is not allocated again on the next frame.
            view.releaseCache(); view.failAllocationsForTest(true);
            check("list-alloc-fail",m,d); direct("list-image-alloc-fail",m,d);
            {
                const auto failed=view.cacheStats().failures;
                renderer.invalidate(); renderer.draw(m,d);
                renderer.invalidate(); renderer.draw(m,d);
                ++checks;
                if(!failed || view.cacheStats().failures!=failed) {
                    ++failures;
                    std::printf("[Verify] FAIL list image allocation retried: failures=%u then %u\n",
                        unsigned(failed),unsigned(view.cacheStats().failures));
                }
            }
            view.failAllocationsForTest(false); view.releaseCache();
            check("list-alloc-recovered",m,d); direct("list-image-recovered",m,d);
            m.launcher.names[2]=nullptr;
            for(int i=0;i<5;++i) m.launcher.rowDimmed[i]=false;
            // The external screen: every state a slot can report, then the two
            // phases the boot commit adds. The injected catalog is what makes
            // a corrupt slot reachable without breaking a real one.
            // Ready in Browsing is reachable too: open a slot while it is still
            // being verified and let the scan finish underneath it.
            m.screen=ScreenId::External;
            for(int slot=1;slot<=SlotCount;++slot) {
                const auto& entry=catalog.slots[slot-1];
                m.external=ExternalModel{}; m.external.slot=slot; m.external.status=entry.status;
                m.external.name=entry.name[0] ? entry.name : nullptr;
                m.external.version=entry.version[0] ? entry.version : nullptr;
                m.external.error=entry.error;
                check("external-browse",m,d);
                vTaskDelay(1);
            }
            for(const auto status:{SlotStatus::Scanning,SlotStatus::ReadError,SlotStatus::Unsupported}) {
                m.external=ExternalModel{}; m.external.slot=2; m.external.status=status;
                m.external.error=status==SlotStatus::ReadError ? 0x102 : 0;
                check("external-state",m,d);
            }
            m.external=ExternalModel{}; m.external.slot=1; m.external.status=SlotStatus::Ready;
            m.external.name=catalog.slots[0].name; m.external.version=catalog.slots[0].version;
            m.external.phase=ExternalPhase::BootCommitting; check("external-committing",m,d);
            m.external.phase=ExternalPhase::BootFailed;
            m.external.message="ESP_ERR_IMAGE_INVALID"; check("external-failed",m,d);
            // A guest chooses its own name, so the detail has to survive one
            // that is too long and holds a glyph the subset does not carry.
            m.external=ExternalModel{}; m.external.slot=1; m.external.status=SlotStatus::Ready;
            m.external.name="非常に長い外部アプリ名と未収録文字😀";
            m.external.version="1.0.0-verify"; check("external-long-name",m,d);
            m.screen=ScreenId::AppList; m.external=ExternalModel{};
            check("external-left",m,d);
            // The stopwatch. Its panel and divider are background rather than
            // plan elements, so these differential frames are what proves the
            // background survives: an element that skipped a repaint, or one
            // whose box moved, shows up as a black hole against the full
            // repaint the comparison draws.
            m.screen=ScreenId::Stopwatch; m.stopwatch=StopwatchModel{};
            check("stopwatch-reset",m,d);
            for(const auto state:{StopwatchState::Running,StopwatchState::Paused}) {
                m.stopwatch.state=state;
                for(int laps=0;laps<=StopwatchLapRows;++laps) {
                    m.stopwatch.rows=laps;
                    for(int i=0;i<laps;++i) {
                        m.stopwatch.lapNumber[i]=uint16_t(laps-i);
                        m.stopwatch.lapUs[i]=TimeUs(laps-i)*1234567;
                    }
                    check("stopwatch-laps",m,d);
                }
                vTaskDelay(1);
            }
            // 40 Hz: only the hundredths may repaint, and it has to land on the
            // panel colour rather than on the black the erase leaves behind.
            m.stopwatch.state=StopwatchState::Running;
            for(int i=0;i<12;++i) {
                m.stopwatch.elapsedUs+=StopwatchFrameUs;
                check("stopwatch-hundredths",m,d);
            }
            // A carry into every field, and the display cap.
            for(const TimeUs value:{TimeUs(0),TimeUs(9990000),TimeUs(59990000),
                                    TimeUs(3599990000LL),StopwatchDisplayCapUs,
                                    StopwatchDisplayCapUs+60000000}) {
                m.stopwatch.elapsedUs=value;
                check("stopwatch-value",m,d);
            }
            // The toast is the only other thing that touches these pixels, so
            // it has to force a full repaint on the way in and on the way out.
            m.toast="保存しました"; check("stopwatch-toast-on",m,d);
            m.toast=nullptr; check("stopwatch-toast-off",m,d);
            m.screen=ScreenId::AppList; m.stopwatch=StopwatchModel{};
            check("stopwatch-left",m,d);
            // The timer (work 12-3): the setup with the focus on every field
            // and on SET (which greys the keys), typed values, the countdown
            // with RESET filling, and the alert counting up. Each view is
            // entered from another, which is a full repaint of the layer.
            m.screen=ScreenId::Timer; m.timer=TimerModel{};
            m.timer.fields[1]=3; check("timer-setup",m,d);
            for(const int focus:{2,3,0}) { m.timer.focus=focus; check("timer-focus",m,d); }
            m.timer.focus=1; check("timer-focus-wrap",m,d);
            for(const int value:{1,12,99,75,0}) { m.timer.fields[0]=value; check("timer-field",m,d); }
            m.timer.fields[2]=60; check("timer-field-over",m,d);
            m.timer.view=TimerView::Countdown; m.timer.seconds=3660; check("timer-countdown",m,d);
            for(const int32_t value:{3659,3600,3599,600,59,10,9,1}) { m.timer.seconds=value; check("timer-second",m,d); }
            m.timer.seconds=TimerMaxSeconds; check("timer-longest",m,d);
            m.timer.paused=true; check("timer-paused",m,d);
            m.timer.paused=false; check("timer-resumed",m,d);
            for(int fill=0;fill<=1000;fill+=125) { m.timer.resetFill=uint16_t(fill); check("timer-reset-fill",m,d); }
            m.timer.resetFill=0; check("timer-reset-empty",m,d);
            vTaskDelay(1);
            m.timer.view=TimerView::Ringing; m.timer.seconds=0; check("timer-ringing",m,d);
            for(const int32_t value:{int32_t(1),int32_t(2),int32_t(59),int32_t(60),int32_t(3600),TimerMaxSeconds}) { m.timer.seconds=value; check("timer-count-up",m,d); }
            m.toast="保存しました"; check("timer-toast-on",m,d);
            m.toast=nullptr; check("timer-toast-off",m,d);
            m.timer=TimerModel{}; m.timer.fields[1]=3; check("timer-setup-again",m,d);
            m.screen=ScreenId::AppList; m.timer=TimerModel{};
            check("timer-left",m,d);
            m={}; m.viewport={w,h}; check("home",m,d);
            d.localTime.tm_min=42; check("minute",m,d);
            d.localTime.tm_mday=20; d.localTime.tm_wday=0; check("date",m,d);
            d.timeValid=false; d.batteryPercent=-1; check("unknown",m,d);
            d=sampleData(); d.charging=true; check("charging",m,d);
            // Work 10-2: the seconds variant, with the cache remade for it,
            // and the way back to minutes.
            {
                HomeEvent hold; hold.kind=HomeEventKind::LongPress;
                renderer.handle(hold); check("seconds",m,d);
                ++d.localTime.tm_sec; check("second-tick",m,d);
                d.localTime.tm_sec=11; check("second-narrow",m,d);
                m.launcher.transition=0.45f; check("seconds-transition",m,d);
                m.launcher.transition=0; d.timeValid=false; check("seconds-unknown",m,d);
                d=sampleData(); d.charging=true;
                renderer.handle(hold); check("minutes-again",m,d);
                const auto before=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
                for(int i=0;i<16;++i) { renderer.handle(hold); renderer.draw(m,d); vTaskDelay(1); }
                std::printf("[Verify] variants internal_free_before=%u after=%u\n",unsigned(before),
                    unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
            }
            // Work 10-3: the battery row, the items and their chips, and the
            // caches against drawing directly (docs/task10/plan-10-3.md 6).
            // Frames are built in static storage: a WatchData is a few hundred
            // bytes and this function already runs deep on the main stack.
            {
                HomeEvent hold; hold.kind=HomeEventKind::LongPress;
                static WatchData shown;
                auto withItems=[&](int count) -> const WatchData& {
                    const char* labels[]={"02:40","12:34","00:07","100:00"};
                    shown=d;
                    for(int i=0;i<count && i<BackgroundCapacity;++i) {
                        auto& item=shown.background.items[i];
                        item=BackgroundInfo{};
                        item.appId=i==0 ? LaunchTargetId::Stopwatch : static_cast<LaunchTargetId>(40+i);
                        std::snprintf(item.label,sizeof(item.label),"%s",labels[i]);
                        item.icon=i%2==0 ? appIcon(IconId::Stopwatch) : nullptr;
                        if(i!=2) item.suggestedColor=i==0 ? StopwatchColors.background : uint16_t(0xfd03);
                    }
                    shown.background.count=uint8_t(std::min(count,BackgroundCapacity));
                    return shown;
                };
                m={}; m.viewport={w,h}; d=sampleData();
                for(int percent:{0,30,100,-1}) { d.batteryPercent=percent; check("battery-level",m,d); }
                d.batteryPercent=82;
                d.charging=true; check("battery-charging",m,d);
                d.charging=false; check("battery-discharging",m,d);
                check("item-added",m,withItems(1));
                check("item-second",m,withItems(2));
                std::snprintf(shown.background.items[0].label,BackgroundLabelBytes,"02:41"); check("item-label",m,shown);
                shown.background.items[1].suggestedColor=uint16_t(0x0000); check("item-colour",m,shown);
                shown.background.items[1].icon=appIcon(IconId::Settings); check("item-icon",m,shown);
                std::snprintf(shown.background.items[0].label,BackgroundLabelBytes,
                    "A long label the chip has to shorten, 999:59");
                check("item-long",m,shown);
                std::snprintf(shown.background.items[1].label,BackgroundLabelBytes,"計測中");
                check("item-japanese",m,shown);
                check("item-four",m,withItems(4));           // two drawn
                check("item-removed",m,withItems(1));
                check("item-none",m,d);
                withItems(2);
                m.toast="保存しました"; check("item-toast-on",m,shown);
                m.toast=nullptr; check("item-toast-off",m,shown);
                m.launcher.transition=0.45f; check("item-transition",m,shown);
                m.launcher.transition=0; check("item-transition-back",m,shown);
                renderer.handle(hold);
                for(int s=0;s<3;++s) { ++shown.localTime.tm_sec; check("item-second-tick",m,shown); }
                shown.localTime.tm_min=59; shown.localTime.tm_sec=59; check("item-minute-edge",m,shown);
                shown.localTime.tm_hour=23; check("item-hour-edge",m,shown);
                shown.localTime.tm_hour=0; shown.localTime.tm_min=0; shown.localTime.tm_sec=0;
                shown.localTime.tm_mday=20; shown.localTime.tm_wday=0; check("item-day-edge",m,shown);
                shown.timeValid=false; check("item-unknown-time",m,shown);
                renderer.handle(hold);
                // The same frames from the caches and drawn directly.
                auto faceDirect=[&](const char* name,const FrameModel& fm,const WatchData& fd) {
                    renderer.invalidate(); renderer.draw(fm,fd); display.readRect(0,0,w,h,incremental);
                    renderer.selectFace("digital",true); renderer.draw(fm,fd); display.readRect(0,0,w,h,reference);
                    renderer.selectFace("digital");
                    compare(name);
                };
                m={}; m.viewport={w,h}; d=sampleData();
                faceDirect("digital-direct",m,d);
                faceDirect("digital-direct-items",m,withItems(2));
                renderer.handle(hold);
                faceDirect("digital-direct-seconds",m,withItems(1));
                renderer.handle(hold);
            }
            display.fillScreen(0x1234); renderer.invalidate(); check("wake-invalidate",m,d);
            renderer.selectFace("digital",true); check("cache-disabled",m,d);
            m.launcher.transition=0.45f; check("cache-disabled-transition",m,d);
            renderer.capacityForTest(2); check("capacity-overflow",m,d);
            renderer.capacityForTest(FramePlan::Capacity); check("capacity-recovery",m,d);
            static TestFace alternate;
            renderer.registerFace(alternate); renderer.selectFace(alternate.id());
            check("alternate-face",m,d); ++d.localTime.tm_min; check("overlap-foreground",m,d);
            renderer.selectFace("digital"); m.launcher.transition=0; check("digital-restored",m,d);
            // Work 10-4: the damage over scenery, the list's own background
            // over the face, and the list's names on that background, against
            // full repaints (docs/task10/plan-10-4.md 7). Twice: on the black
            // list background and on a coloured one.
            static BackdropFace backdrop;
            renderer.registerFace(backdrop);
            {
                static WatchData scene;
                auto info=[&](const char* label) {
                    auto& it=scene.background.items[0];
                    it=BackgroundInfo{}; it.appId=LaunchTargetId::Stopwatch;
                    std::snprintf(it.label,sizeof(it.label),"%s",label);
                    scene.background.count=1;
                };
                FrameModel bm;
                for(const uint16_t colour:{uint16_t(0x0000),uint16_t(0x18c9)}) {
                    backdrop.listBackgroundForTest(colour);
                    renderer.selectFace(backdrop.id());
                    bm=FrameModel{}; bm.viewport={w,h}; scene=sampleData();
                    check("backdrop",bm,scene);
                    ++scene.localTime.tm_min; check("backdrop-minute",bm,scene);
                    scene.localTime.tm_hour=11; scene.localTime.tm_min=11; check("backdrop-narrower",bm,scene);
                    info("02:40"); check("backdrop-info-on",bm,scene);
                    info("12:41"); check("backdrop-info-label",bm,scene);
                    bm.toast="保存しました"; check("backdrop-toast-on",bm,scene);
                    bm.toast=nullptr; check("backdrop-toast-off",bm,scene);
                    scene.background.count=0; check("backdrop-info-off",bm,scene);
                    scene.timeValid=false; check("backdrop-unknown",bm,scene);
                    scene=sampleData(); check("backdrop-known",bm,scene);
                    // The list rising over it: a hair, half, nearly and fully
                    // up, turning back half way, and down to rest again.
                    bm.screen=ScreenId::AppList;
                    for(const float p:{0.004f,0.02f,0.25f,0.5f,0.75f,0.98f,0.998f,1.0f,0.7f,0.3f,0.6f,0.05f,0.0f}) {
                        bm.launcher.transition=p; check("backdrop-transition",bm,scene);
                    }
                    vTaskDelay(1);
                    // Rows over the background: selections, positions between
                    // rows, the names' images against the glyphs, a notice.
                    bm.launcher.transition=1;
                    for(int i=0;i<5;++i) {
                        bm.launcher.list.selection=i;
                        bm.launcher.list.scroll=float(i*rowSpacing(bm.viewport)+(i%2)*29);
                        check("backdrop-list",bm,scene);
                    }
                    direct("backdrop-list-image",bm,scene);
                    bm.toast="準備中"; check("backdrop-list-toast-on",bm,scene);
                    bm.toast=nullptr; check("backdrop-list-toast-off",bm,scene);
                    // The scenery rearranging under a half raised list.
                    bm.launcher.transition=0.5f; info("00:07"); check("backdrop-info-under-list",bm,scene);
                    scene.background.count=0; check("backdrop-info-gone-under-list",bm,scene);
                    vTaskDelay(1);
                }
                // The face asking for another colour under a shown list: the
                // background, every row and every cached name follow.
                bm.launcher.transition=1; bm.launcher.list.scroll=float(rowSpacing(bm.viewport));
                backdrop.listBackgroundForTest(0x4208); check("backdrop-colour",bm,scene);
                direct("backdrop-colour-image",bm,scene);
                bm.launcher.transition=0.4f; check("backdrop-colour-mid",bm,scene);
                backdrop.listBackgroundForTest(0x0000); check("backdrop-colour-black",bm,scene);
                backdrop.listBackgroundForTest(0x18c9); check("backdrop-colour-again",bm,scene);
                // Names that cannot be cached, drawn straight onto the colour.
                bm.launcher.transition=1;
                view.releaseCache(); view.failAllocationsForTest(true);
                check("backdrop-alloc-fail",bm,scene); direct("backdrop-image-alloc-fail",bm,scene);
                view.failAllocationsForTest(false); view.releaseCache();
                check("backdrop-alloc-recovered",bm,scene);
                // Over capacity mid-slide, and back.
                bm.launcher.transition=0.5f;
                renderer.capacityForTest(2); check("backdrop-overflow",bm,scene);
                renderer.capacityForTest(FramePlan::Capacity); check("backdrop-overflow-recovery",bm,scene);
                // Back to Digital mid-slide: black under the list again.
                renderer.selectFace("digital"); check("backdrop-to-digital",bm,scene);
                bm.launcher.transition=0; bm.screen=ScreenId::Home; check("digital-after-backdrop",bm,d);
                backdrop.listBackgroundForTest(0x18c9);
            }
            // Work 10-5: Forest in its combinations against full repaints,
            // then its caches against drawing directly (docs/task10/plan-10-5.md 7).
            {
                HomeEvent hold; hold.kind=HomeEventKind::LongPress;
                static WatchData fd;
                auto items=[&](int count) {
                    for(int i=0;i<count && i<BackgroundCapacity;++i) {
                        auto& item=fd.background.items[i];
                        item=BackgroundInfo{};
                        item.appId=i==0 ? LaunchTargetId::Stopwatch : static_cast<LaunchTargetId>(40+i);
                        std::snprintf(item.label,sizeof(item.label),"%s",i==0 ? "02:40" : "12:34");
                        item.icon=appIcon(IconId::Stopwatch);
                        item.suggestedColor=StopwatchColors.background;
                    }
                    fd.background.count=uint8_t(std::min(count,BackgroundCapacity));
                };
                FrameModel fm; fm.viewport={w,h};
                if(!renderer.selectFace("forest")) { ++failures; std::printf("[Verify] FAIL forest did not begin\n"); }
                fd=sampleData();                                   // 82%, not charging: no row
                check("forest",fm,fd);
                ++fd.localTime.tm_min; check("forest-minute",fm,fd);
                fd.localTime.tm_hour=11; fd.localTime.tm_min=11; check("forest-narrow",fm,fd);
                // 31 and 30 hide it, 29 shows it; unknown after showing keeps it.
                for(int percent:{31,30,29,5,0,100,-1,30}) { fd.batteryPercent=percent; check("forest-battery",fm,fd); }
                fd.charging=true; check("forest-charging",fm,fd);
                fd.batteryPercent=-1; check("forest-charging-unknown",fm,fd);
                fd.charging=false; fd.batteryPercent=82; check("forest-discharging",fm,fd);
                fd.chargingKnown=false; fd.batteryPercent=12; check("forest-charging-unreadable",fm,fd);
                fd.chargingKnown=true; fd.batteryPercent=82; check("forest-battery-gone",fm,fd);
                items(1); check("forest-item",fm,fd);
                items(2); check("forest-items",fm,fd);
                fd.batteryPercent=18; check("forest-battery-items",fm,fd);
                std::snprintf(fd.background.items[0].label,BackgroundLabelBytes,"02:41"); check("forest-item-label",fm,fd);
                // Forest ignores the suggested colour: a change of it alone draws nothing.
                fd.background.items[1].suggestedColor=uint16_t(0xf800);
                renderer.draw(fm,fd); ++checks;
                if(!renderer.lastDirty().empty()) {
                    ++failures; const Rect r=renderer.lastDirty();
                    std::printf("[Verify] FAIL forest-colour-only repainted %d,%d %dx%d\n",r.x,r.y,r.w,r.h);
                }
                check("forest-colour-only",fm,fd);
                fd.background.items[1].icon=appIcon(IconId::Settings); check("forest-item-icon",fm,fd);
                fd.background.items[1].appId=static_cast<LaunchTargetId>(77); check("forest-unknown-app",fm,fd);
                fd.background.items[1].icon=nullptr; check("forest-item-generic",fm,fd);
                std::snprintf(fd.background.items[0].label,BackgroundLabelBytes,"A long label the row has to shorten, 999:59");
                check("forest-item-long",fm,fd);
                std::snprintf(fd.background.items[1].label,BackgroundLabelBytes,"計測中"); check("forest-item-japanese",fm,fd);
                fm.toast="保存しました"; check("forest-toast-on",fm,fd);
                fm.toast=nullptr; check("forest-toast-off",fm,fd);
                items(1); check("forest-item-removed",fm,fd);
                fd.background.count=0; fd.batteryPercent=82; check("forest-plain-again",fm,fd);
                vTaskDelay(1);
                renderer.handle(hold); check("forest-seconds",fm,fd);
                for(int s=0;s<3;++s) { ++fd.localTime.tm_sec; check("forest-second-tick",fm,fd); }
                fd.localTime.tm_min=59; fd.localTime.tm_sec=59; check("forest-minute-edge",fm,fd);
                fd.localTime.tm_hour=23; check("forest-hour-edge",fm,fd);
                // Each hour's palette repaints the whole face: before dawn,
                // the keys, hours between them and across midnight
                // (docs/forest-gradient/plan.md 4).
                for(int hour:{5,6,7,12,17,19,2,23}) { fd.localTime.tm_hour=hour; check("forest-palette",fm,fd); }
                items(2); fd.batteryPercent=5; check("forest-seconds-info",fm,fd);
                fd.timeValid=false; check("forest-seconds-unknown",fm,fd);
                fd.timeValid=true;
                // The list over Forest, which stays still: a hair, half, nearly
                // and fully up, turning back, and down to rest.
                fm.screen=ScreenId::AppList;
                for(const float p:{0.004f,0.02f,0.25f,0.5f,0.75f,0.98f,1.0f,0.7f,0.3f,0.6f,0.05f,0.0f}) {
                    fm.launcher.transition=p; check("forest-transition",fm,fd);
                }
                // The list on the hour's ground: the hour turning under it,
                // fully and half raised, the rows and their names following.
                fm.launcher.transition=1;
                for(int hour:{23,5,12,18}) { fd.localTime.tm_hour=hour; check("forest-list-hour",fm,fd); }
                fm.launcher.transition=0.5f;
                for(int hour:{19,9}) { fd.localTime.tm_hour=hour; check("forest-list-hour-mid",fm,fd); }
                // The scenery moving under a half raised list.
                fm.launcher.transition=0.5f; fd.background.count=0; fd.batteryPercent=82;
                check("forest-info-gone-under-list",fm,fd);
                items(1); check("forest-info-under-list",fm,fd);
                // A slow drag back to the clock: the edge a pixel or two at a
                // time with no full repaint in between, the seconds ticking
                // now and then, and the finger wavering back up. Only the
                // last frame is compared, so anything a step left behind shows.
                {
                    fd.background.count=0; fd.batteryPercent=82;
                    renderer.handle(hold);
                    auto edge=[&](int top) { fm.launcher.transition=1.0f-(float(top)+0.5f)/float(h); };
                    edge(0); check("forest-slow-start",fm,fd);
                    int top=0,step=0;
                    while (top<h-20) {
                        top+=1+step%2;
                        if (step%7==6) top-=3;          // wavering back up
                        edge(top);
                        if (step%29==0) ++fd.localTime.tm_sec;
                        renderer.draw(fm,fd);
                        if (++step%16==0) vTaskDelay(1);
                    }
                    check("forest-slow-return",fm,fd);
                    // The same through the launcher's own controller: the list
                    // opened, then pulled back down by a slow finger.
                    LauncherController launcher({w,h});
                    TimeUs now=1000000;
                    launcher.openList(now);
                    for (int i=0;i<40;++i) { now+=16000; launcher.update(now); }
                    auto frameAt=[&]() {
                        const auto lm=launcher.model();
                        fm.launcher.transition=lm.transition; fm.launcher.list=lm.list;
                        renderer.draw(fm,fd);
                    };
                    frameAt();
                    Events e{}; e.gesture=Gesture::TouchStart; launcher.handle(e,now);
                    int total=10;
                    e={}; e.gesture=Gesture::DragStart; e.totalY=total; launcher.handle(e,now); frameAt();
                    for (int i=0;i<150;++i) {
                        now+=10000;
                        total+=(i%5==4) ? -1 : 1;
                        e={}; e.gesture=Gesture::DragMove; e.totalY=total; launcher.handle(e,now);
                        if (i%31==0) ++fd.localTime.tm_sec;
                        frameAt();
                        if (i%16==15) vTaskDelay(1);
                    }
                    fm.launcher.transition=launcher.model().transition;
                    check("forest-slow-drag",fm,fd);
                    std::printf("[Verify] forest slow drag ends at transition=%.3f\n",double(fm.launcher.transition));
                    renderer.handle(hold);
                }
                renderer.handle(hold); check("forest-minutes-again",fm,fd);
                vTaskDelay(1);
                auto faceDirect=[&](const char* name) {
                    renderer.invalidate(); renderer.draw(fm,fd); display.readRect(0,0,w,h,incremental);
                    renderer.selectFace("forest",true); renderer.draw(fm,fd); display.readRect(0,0,w,h,reference);
                    renderer.selectFace("forest");
                    compare(name);
                };
                fd=sampleData(); faceDirect("forest-direct");
                items(2); fd.batteryPercent=18; faceDirect("forest-direct-info");
                renderer.handle(hold); faceDirect("forest-direct-seconds"); renderer.handle(hold);
                renderer.capacityForTest(2); check("forest-overflow",fm,fd);
                renderer.capacityForTest(FramePlan::Capacity); check("forest-overflow-recovery",fm,fd);
                // Choosing faces and variants over and over leaks nothing.
                const auto free=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
                for(int i=0;i<16;++i) {
                    renderer.selectFace("digital"); renderer.draw(fm,fd);
                    renderer.selectFace("forest"); renderer.handle(hold); renderer.draw(fm,fd);
                    vTaskDelay(1);
                }
                std::printf("[Verify] faces internal_free_before=%u after=%u\n",unsigned(free),
                    unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
                renderer.selectFace("digital"); check("forest-to-digital",fm,d);
            }
            // Work 11-2: Analog against full repaints: the hands in their ten
            // second steps, the dot at every second, the hands across the date
            // and the row, and the list over it (docs/task11/plan-11-2.md 6).
            {
                HomeEvent hold; hold.kind=HomeEventKind::LongPress;
                static WatchData ad;
                auto items=[&](int count) {
                    for(int i=0;i<count && i<BackgroundCapacity;++i) {
                        auto& item=ad.background.items[i];
                        item=BackgroundInfo{};
                        item.appId=i==0 ? LaunchTargetId::Stopwatch : static_cast<LaunchTargetId>(40+i);
                        std::snprintf(item.label,sizeof(item.label),"%s",i==0 ? "02:40" : "12:34");
                        item.icon=appIcon(IconId::Stopwatch);
                        item.suggestedColor=StopwatchColors.background;
                    }
                    ad.background.count=uint8_t(std::min(count,BackgroundCapacity));
                };
                auto at=[&](int hour,int minute,int second) {
                    ad.localTime.tm_hour=hour; ad.localTime.tm_min=minute; ad.localTime.tm_sec=second;
                };
                // A frame that should send nothing at all.
                auto quiet=[&](const char* name,const FrameModel& m) {
                    renderer.draw(m,ad); ++checks;
                    if(!renderer.lastDirty().empty()) {
                        ++failures; const Rect r=renderer.lastDirty();
                        std::printf("[Verify] FAIL %s repainted %d,%d %dx%d\n",name,r.x,r.y,r.w,r.h);
                    }
                };
                FrameModel am; am.viewport={w,h};
                if(!renderer.selectFace("analog")) { ++failures; std::printf("[Verify] FAIL analog did not begin\n"); }
                if(renderer.analogSeconds()) renderer.handle(hold);
                ad=sampleData();                                   // 9:41:00, 82%: no row
                check("analog",am,ad);
                // Without the dot, seconds inside a step draw nothing; the
                // step moves both hands.
                for(int s=1;s<10;++s) { ad.localTime.tm_sec=s; quiet("analog-quiet-second",am); }
                ad.localTime.tm_sec=10; check("analog-step",am,ad);
                ad.localTime.tm_sec=59; check("analog-step-59",am,ad);
                at(9,42,0); check("analog-minute",am,ad);
                at(11,59,50); check("analog-before-noon",am,ad);
                at(12,0,0); check("analog-noon",am,ad);
                at(23,59,50); check("analog-before-midnight",am,ad);
                at(0,0,0); ad.localTime.tm_mday=20; check("analog-new-day",am,ad);
                ad.localTime.tm_mday=9; check("analog-one-digit-day",am,ad);
                ad.localTime.tm_mday=31; check("analog-two-digit-day",am,ad);
                // The axes and the diagonals, the hands apart and together.
                const int angles[][2]={{3,0},{6,0},{9,0},{12,15},{1,30},{4,30},{7,45},{10,7},{6,32},{3,16}};
                for(const auto& t:angles) { at(t[0],t[1],0); check("analog-angles",am,ad); }
                // The hands over the date, and the date changing under them.
                at(3,15,0); check("analog-over-date",am,ad);
                ad.localTime.tm_mday=19; check("analog-date-under-hands",am,ad);
                at(3,15,10); check("analog-off-date",am,ad);
                // Over the row: its parts appearing, changing and going under
                // the hands.
                at(6,32,0);
                for(int percent:{31,30,29,5,0,100,-1,30}) { ad.batteryPercent=percent; check("analog-battery",am,ad); }
                ad.charging=true; check("analog-charging",am,ad);
                ad.batteryPercent=-1; check("analog-charging-unknown",am,ad);
                ad.charging=false; ad.batteryPercent=82; check("analog-discharging",am,ad);
                ad.chargingKnown=false; ad.batteryPercent=12; check("analog-charging-unreadable",am,ad);
                ad.chargingKnown=true; ad.batteryPercent=82; check("analog-battery-gone",am,ad);
                items(1); check("analog-item",am,ad);
                items(2); check("analog-items",am,ad);
                ad.batteryPercent=18; check("analog-battery-items",am,ad);
                std::snprintf(ad.background.items[0].label,BackgroundLabelBytes,"02:41"); check("analog-item-label",am,ad);
                // The suggested colour is not used: a change of it alone draws nothing.
                ad.background.items[1].suggestedColor=uint16_t(0xf800); quiet("analog-colour-only",am);
                check("analog-colour-only",am,ad);
                ad.background.items[1].icon=appIcon(IconId::Settings); check("analog-item-icon",am,ad);
                ad.background.items[1].icon=nullptr; check("analog-item-generic",am,ad);
                std::snprintf(ad.background.items[0].label,BackgroundLabelBytes,"A long label the row has to shorten, 999:59");
                check("analog-item-long",am,ad);
                std::snprintf(ad.background.items[1].label,BackgroundLabelBytes,"計測中"); check("analog-item-japanese",am,ad);
                at(6,32,10); check("analog-step-over-row",am,ad);
                am.toast="保存しました"; check("analog-toast-on",am,ad);
                am.toast=nullptr; check("analog-toast-off",am,ad);
                items(1); check("analog-item-removed",am,ad);
                ad.background.count=0; ad.batteryPercent=82; check("analog-plain-again",am,ad);
                vTaskDelay(1);
                // The dot: shown, every second of a minute, across the steps,
                // over the date, then hidden again.
                at(9,41,0);
                renderer.handle(hold); check("analog-seconds",am,ad);
                for(int s=1;s<60;++s) {
                    ad.localTime.tm_sec=s; check("analog-second",am,ad);
                    if(s%8==0) vTaskDelay(1);
                }
                at(9,42,0); check("analog-second-minute",am,ad);
                at(3,15,15); check("analog-second-at-date",am,ad);     // the minute hand over the date, the dot beyond it
                items(2); ad.batteryPercent=5; at(6,30,30); check("analog-second-info",am,ad);
                ad.timeValid=false; check("analog-seconds-unknown",am,ad);
                ad.timeValid=true; check("analog-seconds-known",am,ad);
                renderer.handle(hold); check("analog-seconds-hidden",am,ad);
                ad.background.count=0; ad.batteryPercent=82;
                ad.timeValid=false; check("analog-unknown",am,ad);
                ad.timeValid=true; check("analog-known",am,ad);
                vTaskDelay(1);
                // The list over Analog, which stays still, with the dot ticking.
                renderer.handle(hold);
                items(1);
                am.screen=ScreenId::AppList;
                for(const float p:{0.004f,0.02f,0.25f,0.5f,0.75f,0.98f,1.0f,0.7f,0.3f,0.6f,0.05f,0.0f}) {
                    am.launcher.transition=p; ++ad.localTime.tm_sec; ad.localTime.tm_sec%=60;
                    check("analog-transition",am,ad);
                }
                am.launcher.transition=0.5f; ad.background.count=0; check("analog-info-gone-under-list",am,ad);
                items(2); check("analog-info-under-list",am,ad);
                at(9,41,50); check("analog-step-under-list",am,ad);
                // A slow return: only the last frame is compared, so anything a
                // step left behind shows.
                {
                    auto edge=[&](int top) { am.launcher.transition=1.0f-(float(top)+0.5f)/float(h); };
                    edge(0); check("analog-slow-start",am,ad);
                    int top=0,step=0;
                    while (top<h-20) {
                        top+=1+step%2;
                        if (step%7==6) top-=3;
                        edge(top);
                        if (step%29==0) { ++ad.localTime.tm_sec; ad.localTime.tm_sec%=60; }
                        renderer.draw(am,ad);
                        if (++step%16==0) vTaskDelay(1);
                    }
                    check("analog-slow-return",am,ad);
                }
                am.screen=ScreenId::Home; am.launcher.transition=0; check("analog-rest",am,ad);
                renderer.handle(hold);
                renderer.selectFace("analog",true); check("analog-reselected",am,ad);
                renderer.capacityForTest(2); check("analog-overflow",am,ad);
                renderer.capacityForTest(FramePlan::Capacity); check("analog-overflow-recovery",am,ad);
                // Choosing faces and the dot over and over leaks nothing.
                const auto free=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
                for(int i=0;i<16;++i) {
                    renderer.selectFace("digital"); renderer.draw(am,ad);
                    renderer.selectFace("forest"); renderer.draw(am,ad);
                    renderer.selectFace("analog"); renderer.handle(hold); renderer.draw(am,ad);
                    vTaskDelay(1);
                }
                std::printf("[Verify] analog faces internal_free_before=%u after=%u\n",unsigned(free),
                    unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
                if(renderer.analogSeconds()) renderer.handle(hold);
                renderer.selectFace("digital"); check("analog-to-digital",am,d);
            }
            // Work 11-3: Noonish against full repaints: its regions turning
            // at every step, the hands meeting and passing, text, icons and the
            // dot across boundaries, and the list over it (docs/task11/plan-11-3.md 7).
            {
                HomeEvent hold; hold.kind=HomeEventKind::LongPress;
                static WatchData nd;
                auto items=[&](int count) {
                    for(int i=0;i<count && i<BackgroundCapacity;++i) {
                        auto& item=nd.background.items[i];
                        item=BackgroundInfo{};
                        item.appId=i==0 ? LaunchTargetId::Stopwatch : static_cast<LaunchTargetId>(40+i);
                        std::snprintf(item.label,sizeof(item.label),"%s",i==0 ? "02:40" : "12:34");
                        item.icon=appIcon(IconId::Stopwatch);
                        item.suggestedColor=StopwatchColors.background;
                    }
                    nd.background.count=uint8_t(std::min(count,BackgroundCapacity));
                };
                auto at=[&](int hour,int minute,int second) {
                    nd.localTime.tm_hour=hour; nd.localTime.tm_min=minute; nd.localTime.tm_sec=second;
                };
                auto quiet=[&](const char* name,const FrameModel& m) {
                    renderer.draw(m,nd); ++checks;
                    if(!renderer.lastDirty().empty()) {
                        ++failures; const Rect r=renderer.lastDirty();
                        std::printf("[Verify] FAIL %s repainted %d,%d %dx%d\n",name,r.x,r.y,r.w,r.h);
                    }
                };
                FrameModel nm; nm.viewport={w,h};
                if(!renderer.selectFace("noonish")) { ++failures; std::printf("[Verify] FAIL noonish did not begin\n"); }
                if(renderer.noonishSeconds()) renderer.handle(hold);
                nd=sampleData(); at(10,7,0);
                check("noonish",nm,nd);
                for(int s=1;s<10;++s) { nd.localTime.tm_sec=s; quiet("noonish-quiet-second",nm); }
                nd.localTime.tm_sec=10; check("noonish-step",nm,nd);
                at(10,8,0); check("noonish-minute",nm,nd);
                // Together, opposite, and the minute hand passing the hour hand
                // (3:16:22) step by step.
                at(12,0,0); check("noonish-together",nm,nd);
                at(6,0,0); check("noonish-opposite",nm,nd);
                for(int s=90;s<=110;++s) { at(3,s/6,(s%6)*10); check("noonish-overtake",nm,nd); vTaskDelay(1); }
                const int angles[][2]={{3,0},{9,0},{12,15},{1,30},{4,30},{7,45},{11,59},{6,32},{3,16},{9,45}};
                for(const auto& t:angles) { at(t[0],t[1],50); check("noonish-angles",nm,nd); vTaskDelay(1); }
                // The date across a boundary and under the hands, changing.
                at(3,15,0); nd.localTime.tm_mday=19; check("noonish-over-date",nm,nd);
                nd.localTime.tm_mday=20; check("noonish-date-change",nm,nd);
                at(2,15,0); check("noonish-date-boundary",nm,nd);       // the hour line through the date
                // The row across boundaries: the hands at 7:30 and 4:40 split it.
                at(7,30,0);
                for(int percent:{31,29,5,-1,82}) { nd.batteryPercent=percent; check("noonish-battery",nm,nd); }
                vTaskDelay(1);
                nd.charging=true; nd.batteryPercent=18; check("noonish-charging",nm,nd);
                nd.charging=false;
                items(1); check("noonish-item",nm,nd);
                items(2); check("noonish-items",nm,nd);
                std::snprintf(nd.background.items[0].label,BackgroundLabelBytes,"02:41"); check("noonish-item-label",nm,nd);
                nd.background.items[1].suggestedColor=uint16_t(0xf800); quiet("noonish-colour-only",nm);
                nd.background.items[1].icon=nullptr; check("noonish-item-generic",nm,nd);
                std::snprintf(nd.background.items[0].label,BackgroundLabelBytes,"A long label the row has to shorten, 999:59");
                check("noonish-item-long",nm,nd);
                std::snprintf(nd.background.items[1].label,BackgroundLabelBytes,"計測中"); check("noonish-item-japanese",nm,nd);
                // The regions turning under a row that stays.
                for(int s=0;s<6;++s) { at(4,40,s*10); check("noonish-row-under-step",nm,nd); vTaskDelay(1); }
                nm.toast="保存しました"; check("noonish-toast-on",nm,nd);
                nm.toast=nullptr; check("noonish-toast-off",nm,nd);
                items(1); check("noonish-item-removed",nm,nd);
                vTaskDelay(1);
                // The dot: every second over the regions, on a boundary at
                // 12:00, and at a step together with the regions turning.
                at(10,7,0);
                renderer.handle(hold); check("noonish-seconds",nm,nd);
                for(int s=1;s<60;++s) {
                    nd.localTime.tm_sec=s; check("noonish-second",nm,nd);
                    vTaskDelay(1);
                }
                at(12,0,0); check("noonish-dot-on-boundary",nm,nd);
                for(int s=1;s<=3;++s) { nd.localTime.tm_sec=s; check("noonish-dot-leaves-boundary",nm,nd); }
                at(11,59,50); check("noonish-dot-step",nm,nd);
                at(12,0,0); check("noonish-dot-step-noon",nm,nd);
                // Unknown: the resting split, no hands, no dot; then known again.
                nd.timeValid=false; check("noonish-unknown",nm,nd);
                at(10,7,0); nd.timeValid=true; check("noonish-known-at-rest",nm,nd);
                at(10,8,30); check("noonish-known",nm,nd);
                renderer.handle(hold); check("noonish-seconds-hidden",nm,nd);
                vTaskDelay(1);
                // The list over Noonish: its edge moving over the regions.
                renderer.handle(hold);
                nm.screen=ScreenId::AppList;
                for(const float p:{0.004f,0.02f,0.25f,0.5f,0.75f,0.98f,1.0f,0.7f,0.3f,0.6f,0.05f,0.0f}) {
                    nm.launcher.transition=p; ++nd.localTime.tm_sec; nd.localTime.tm_sec%=60;
                    check("noonish-transition",nm,nd); vTaskDelay(1);
                }
                nm.launcher.transition=0.5f; at(10,8,50); check("noonish-step-under-list",nm,nd);
                nd.background.count=0; check("noonish-info-gone-under-list",nm,nd);
                {
                    auto edge=[&](int top) { nm.launcher.transition=1.0f-(float(top)+0.5f)/float(h); };
                    edge(0); check("noonish-slow-start",nm,nd);
                    int top=0,step=0;
                    while (top<h-20) {
                        top+=1+step%2;
                        if (step%7==6) top-=3;
                        edge(top);
                        if (step%29==0) { ++nd.localTime.tm_sec; nd.localTime.tm_sec%=60; }
                        renderer.draw(nm,nd);
                        if (++step%16==0) vTaskDelay(1);
                    }
                    check("noonish-slow-return",nm,nd);
                }
                nm.screen=ScreenId::Home; nm.launcher.transition=0; check("noonish-rest",nm,nd);
                // Analog and Noonish keep their own dot: showing Noonish's
                // leaves Analog's as it was, and back.
                {
                    renderer.selectFace("analog");
                    const bool analogDot=renderer.analogSeconds();
                    renderer.selectFace("noonish");
                    const bool noonishDot=renderer.noonishSeconds();
                    renderer.handle(hold);
                    renderer.selectFace("analog"); ++checks;
                    if(renderer.analogSeconds()!=analogDot || renderer.noonishSeconds()==noonishDot) {
                        ++failures; std::printf("[Verify] FAIL noonish and analog share the dot\n");
                    }
                    check("noonish-to-analog",nm,nd);
                    renderer.selectFace("noonish"); check("analog-to-noonish",nm,nd);
                    renderer.handle(hold);
                }
                renderer.selectFace("noonish",true); check("noonish-reselected",nm,nd);
                renderer.capacityForTest(2); check("noonish-overflow",nm,nd);
                renderer.capacityForTest(FramePlan::Capacity); check("noonish-overflow-recovery",nm,nd);
                const auto free=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
                for(int i=0;i<16;++i) {
                    renderer.selectFace("analog"); renderer.draw(nm,nd);
                    renderer.selectFace("noonish"); renderer.handle(hold); renderer.draw(nm,nd);
                    vTaskDelay(1);
                }
                std::printf("[Verify] noonish faces internal_free_before=%u after=%u\n",unsigned(free),
                    unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
                if(renderer.noonishSeconds()) renderer.handle(hold);
                renderer.selectFace("digital"); check("noonish-to-digital",nm,d);
            }
            // Repeated cache release/recreation gives before/after heap evidence.
            const auto before=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
            for(int i=0;i<16;++i) { renderer.selectFace("test-overlap"); renderer.selectFace("digital"); vTaskDelay(1); }
            std::printf("[Verify] lifecycle internal_free_before=%u after=%u\n",unsigned(before),
                unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
            {
                const auto& c=view.cacheStats();
                std::printf("[Verify] list cache bytes=%u allocations=%u failures=%u fits=%u renders=%u\n",
                    unsigned(c.bytes),unsigned(c.allocations),unsigned(c.failures),unsigned(c.fits),unsigned(c.renders));
            }
            std::printf("[Verify] checks=%u mismatches=%u result=%s\n",checks,failures,failures ? "FAIL" : "PASS");
            // Work 10-3: how long Digital's frames take and how much of the
            // panel they send, per kind of update, with no input involved.
            {
                HomeEvent hold; hold.kind=HomeEventKind::LongPress;
                static WatchData frame;
                auto run=[&](const char* name,const FrameModel& fm,int frames,auto&& step) {
                    renderer.invalidate(); renderer.draw(fm,frame);
                    TimeUs total=0,longest=0; uint64_t area=0; unsigned painted=0;
                    for(int i=0;i<frames;++i) {
                        step(i);
                        const TimeUs start=esp_timer_get_time();
                        renderer.draw(fm,frame);
                        const TimeUs spent=esp_timer_get_time()-start;
                        total+=spent; longest=std::max(longest,spent);
                        const Rect dirty=renderer.lastDirty();
                        if(!dirty.empty()) { ++painted; area+=uint64_t(dirty.w)*dirty.h; }
                        if(i%8==7) vTaskDelay(1);
                    }
                    std::printf("[Perf] %s frames=%d painted=%u avg_us=%lld max_us=%lld avg_dirty_px=%llu\n",name,frames,painted,
                        (long long)(total/frames),(long long)longest,(unsigned long long)(painted ? area/painted : 0));
                };
                auto stopwatchItem=[&](int seconds) {
                    auto& item=frame.background.items[0];
                    item=BackgroundInfo{}; item.appId=LaunchTargetId::Stopwatch;
                    std::snprintf(item.label,sizeof(item.label),"%02d:%02d",seconds/60%60,seconds%60);
                    item.icon=appIcon(IconId::Stopwatch); item.suggestedColor=StopwatchColors.background;
                    frame.background.count=1;
                };
                FrameModel pm; pm.viewport={w,h};
                frame=sampleData();
                run("digital-static",pm,30,[&](int) {});
                run("digital-minute",pm,30,[&](int i) { frame.localTime.tm_min=i%60; });
                stopwatchItem(0);
                run("digital-item-second",pm,60,[&](int i) { stopwatchItem(i+1); });
                renderer.handle(hold);
                frame=sampleData();
                run("digital-second",pm,60,[&](int i) { frame.localTime.tm_sec=i%60; });
                stopwatchItem(0);
                run("digital-second-item",pm,60,[&](int i) { frame.localTime.tm_sec=i%60; stopwatchItem(i+1); });
                renderer.handle(hold);
                frame=sampleData(); stopwatchItem(160);
                run("digital-transition",pm,40,[&](int i) { pm.launcher.transition=float(i<20 ? i : 39-i)/20; });
                pm.launcher.transition=0;
                // Work 10-4: the list scrolling on black and on a colour, and
                // a small number changing over scenery (docs/task10/plan-10-4.md 7).
                frame=sampleData();
                auto scroll=[&](int i) { pm.launcher.list.scroll=float((i*7)%(4*rowSpacing(pm.viewport)+1)); };
                pm.screen=ScreenId::AppList; pm.launcher.transition=1;
                run("list-scroll",pm,60,scroll);
                renderer.selectFace(backdrop.id());
                run("backdrop-list-scroll",pm,60,scroll);
                // On Forest's ground, which is the frame's base too.
                renderer.selectFace("forest");
                run("forest-list-scroll",pm,60,scroll);
                renderer.selectFace(backdrop.id());
                pm.screen=ScreenId::Home; pm.launcher.transition=0; pm.launcher.list.scroll=0;
                run("backdrop-minute",pm,30,[&](int i) { frame.localTime.tm_min=i%60; });
                run("backdrop-transition",pm,40,[&](int i) { pm.launcher.transition=float(i<20 ? i : 39-i)/20; });
                backdrop.listBackgroundForTest(0x0000);
                run("backdrop-transition-black",pm,40,[&](int i) { pm.launcher.transition=float(i<20 ? i : 39-i)/20; });
                backdrop.listBackgroundForTest(0x18c9);
                pm.launcher.transition=0;
                renderer.selectFace("digital");
                // What restoring the black base costs, without the transfer.
                {
                    display.startWrite();
                    const TimeUs start=esp_timer_get_time();
                    display.fillRect(0,0,w,h,0);
                    const TimeUs spent=esp_timer_get_time()-start;
                    display.endWrite();
                    renderer.invalidate(); renderer.draw(pm,frame);
                    std::printf("[Perf] base-fill px=%d us=%lld (no transfer)\n",w*h,(long long)spent);
                }
                // Work 10-5: Forest's updates, and full repaints of both faces.
                renderer.selectFace("forest");
                frame=sampleData();
                run("forest-minute",pm,30,[&](int i) { frame.localTime.tm_min=i%60; });
                stopwatchItem(0);
                run("forest-item-second",pm,60,[&](int i) { stopwatchItem(i+1); });
                renderer.handle(hold);
                frame=sampleData();
                run("forest-second",pm,60,[&](int i) { frame.localTime.tm_sec=i%60; });
                stopwatchItem(0);
                run("forest-second-item",pm,60,[&](int i) { frame.localTime.tm_sec=i%60; stopwatchItem(i+1); });
                renderer.handle(hold);
                frame=sampleData(); stopwatchItem(160);
                run("forest-transition",pm,40,[&](int i) { pm.launcher.transition=float(i<20 ? i : 39-i)/20; });
                pm.launcher.transition=0;
                run("forest-full",pm,20,[&](int) { renderer.invalidate(); });
                std::printf("[Perf] forest caches=%d/3\n",renderer.forestCachedParts());
                // Work 11-2: Analog's dot alone, its hands' ten second steps
                // (long diagonal boxes), and both at once (docs/task11/plan.md 4).
                renderer.selectFace("analog");
                if(renderer.analogSeconds()) renderer.handle(hold);
                auto step=[&](int i) { const int k=i+1; frame.localTime.tm_sec=(k%6)*10; frame.localTime.tm_min=41+k/6; };
                frame=sampleData();
                run("analog-static",pm,30,[&](int) {});
                run("analog-step",pm,30,step);
                stopwatchItem(0);
                run("analog-item-second",pm,60,[&](int i) { stopwatchItem(i+1); });
                renderer.handle(hold);
                frame=sampleData();
                // Inside one ten second step: the dot alone, never the hands.
                run("analog-dot",pm,54,[&](int i) { frame.localTime.tm_sec=1+i%9; });
                run("analog-second-step",pm,30,step);
                stopwatchItem(0);
                run("analog-second-item",pm,60,[&](int i) { frame.localTime.tm_sec=i%60; stopwatchItem(i+1); });
                renderer.handle(hold);
                frame=sampleData(); stopwatchItem(160);
                run("analog-transition",pm,40,[&](int i) { pm.launcher.transition=float(i<20 ? i : 39-i)/20; });
                pm.launcher.transition=0;
                run("analog-full",pm,20,[&](int) { renderer.invalidate(); });
                // Work 11-3: Noonish's steps turn the regions, so they repaint
                // the whole face; the dot alone lightens a small square.
                renderer.selectFace("noonish");
                if(renderer.noonishSeconds()) renderer.handle(hold);
                frame=sampleData();
                run("noonish-static",pm,30,[&](int) {});
                run("noonish-step",pm,30,step);
                stopwatchItem(0);
                run("noonish-item-second",pm,60,[&](int i) { stopwatchItem(i+1); });
                renderer.handle(hold);
                frame=sampleData();
                run("noonish-dot",pm,54,[&](int i) { frame.localTime.tm_sec=1+i%9; });
                run("noonish-second-step",pm,30,step);
                stopwatchItem(0);
                run("noonish-second-item",pm,60,[&](int i) { frame.localTime.tm_sec=i%60; stopwatchItem(i+1); });
                renderer.handle(hold);
                frame=sampleData(); stopwatchItem(160);
                run("noonish-transition",pm,40,[&](int i) { pm.launcher.transition=float(i<20 ? i : 39-i)/20; });
                pm.launcher.transition=0;
                run("noonish-full",pm,20,[&](int) { renderer.invalidate(); });
                renderer.selectFace("digital");
                run("digital-full",pm,20,[&](int) { renderer.invalidate(); });
                std::printf("[Perf] digital caches=%d/5 internal_free=%u largest=%u\n",renderer.digitalCachedParts(),
                    unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
                    unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
            }
#ifdef LAUNCHER_RENDER_SHOTS
            // Pictures of the faces for looking at on the PC
            // (tools/render_shots.py), after the check so they change nothing.
            {
                auto shoot=[&](const char* name,const FrameModel& fm,const WatchData& fd) {
                    renderer.invalidate(); renderer.draw(fm,fd); display.readRect(0,0,w,h,incremental);
                    dumpShot(name,incremental,w,h);
                };
                HomeEvent hold; hold.kind=HomeEventKind::LongPress;
                static WatchData scene;
                auto item=[](int i,const char* label,const IconBitmap* icon,std::optional<uint16_t> colour) {
                    auto& it=scene.background.items[i];
                    it=BackgroundInfo{}; it.appId=static_cast<LaunchTargetId>(i==0 ? 0 : 41);
                    std::snprintf(it.label,sizeof(it.label),"%s",label);
                    it.icon=icon; it.suggestedColor=colour;
                    scene.background.count=uint8_t(std::max<int>(scene.background.count,i+1));
                };
                display.fillScreen(0xf800); display.readRect(0,0,1,1,incremental);
                std::printf("[ShotProbe] red=%04x\n",unsigned(incremental[0]));
                FrameModel sm; sm.viewport={w,h};
                scene=sampleData(); shoot("digital",sm,scene);
                item(0,"02:40",appIcon(IconId::Stopwatch),uint16_t(0xfd03));
                item(1,"02:40",appIcon(IconId::Stopwatch),StopwatchColors.background);
                shoot("digital-items",sm,scene);
                renderer.handle(hold);
                scene=sampleData(); shoot("digital-seconds",sm,scene);
                item(0,"12:34",appIcon(IconId::Stopwatch),StopwatchColors.background);
                shoot("digital-seconds-item",sm,scene);
                renderer.handle(hold);
                scene=sampleData(); scene.timeValid=false; scene.batteryPercent=-1;
                shoot("digital-unknown",sm,scene);
                scene=sampleData(); scene.charging=true; scene.batteryPercent=5;
                item(0,"A very long label the chip has to shorten",nullptr,std::nullopt);
                item(1,"計測中",appIcon(IconId::Settings),uint16_t(0x0000));
                shoot("digital-mixed",sm,scene);
                scene=sampleData(); scene.batteryPercent=100;
                item(0,"100:00",appIcon(IconId::Stopwatch),StopwatchColors.background);
                shoot("digital-full",sm,scene);
                scene=sampleData();
                item(0,"02:40",appIcon(IconId::Stopwatch),uint16_t(0xfd03));
                item(1,"02:40",appIcon(IconId::Stopwatch),StopwatchColors.background);
                sm.launcher.transition=0.45f; shoot("digital-transition",sm,scene);
                // Work 10-4: the list laid over scenery, on black and on a
                // colour, drawn differentially after a slide from rest.
                auto slide=[&](const char* name,float to) {
                    sm.screen=ScreenId::AppList; sm.launcher.transition=0;
                    renderer.invalidate(); renderer.draw(sm,scene);
                    for(int i=1;i<=10;++i) { sm.launcher.transition=to*float(i)/10; renderer.draw(sm,scene); }
                    display.readRect(0,0,w,h,incremental); dumpShot(name,incremental,w,h);
                };
                renderer.selectFace(backdrop.id());
                scene=sampleData();
                backdrop.listBackgroundForTest(0x0000); slide("backdrop-transition",0.45f);
                backdrop.listBackgroundForTest(0x18c9); slide("backdrop-transition-colour",0.6f);
                item(0,"02:40",appIcon(IconId::Stopwatch),StopwatchColors.background);
                sm.screen=ScreenId::Home; sm.launcher.transition=0; shoot("backdrop-info",sm,scene);
                // Work 10-5: Forest, like the reference pictures and around them.
                renderer.selectFace("forest");
                scene=sampleData(); shoot("forest",sm,scene);
                scene.batteryPercent=18;
                item(0,"02:40",appIcon(IconId::Stopwatch),StopwatchColors.background);
                item(1,"02:40",appIcon(IconId::Stopwatch),uint16_t(0xfd03));
                shoot("forest-info",sm,scene);
                // The provisional palettes through the day (docs/forest-gradient/plan.md 4).
                for(int hour:{5,6,7,17,18,19,22}) {
                    char name[24]; std::snprintf(name,sizeof(name),"forest-hour-%02d",hour);
                    scene.localTime.tm_hour=hour; shoot(name,sm,scene);
                }
                renderer.handle(hold);
                scene=sampleData(); shoot("forest-seconds",sm,scene);
                scene.charging=true; item(0,"12:34",appIcon(IconId::Stopwatch),StopwatchColors.background);
                shoot("forest-seconds-info",sm,scene);
                renderer.handle(hold);
                scene=sampleData(); scene.timeValid=false; scene.batteryPercent=-1; scene.charging=true;
                shoot("forest-unknown",sm,scene);
                scene=sampleData(); scene.batteryPercent=100; scene.charging=true;
                item(0,"A very long label the row has to shorten",nullptr,std::nullopt);
                item(1,"計測中",appIcon(IconId::Settings),uint16_t(0x0000));
                shoot("forest-mixed",sm,scene);
                scene=sampleData(); item(0,"02:40",appIcon(IconId::Stopwatch),StopwatchColors.background);
                slide("forest-transition",0.45f);
                // The list fully up on each part of the day's ground.
                for(int hour:{12,6,18,22}) {
                    char name[24]; std::snprintf(name,sizeof(name),"forest-list-%02d",hour);
                    scene.localTime.tm_hour=hour; slide(name,1.0f);
                }
                sm.screen=ScreenId::Home; sm.launcher.transition=0;
                // Work 11-2: Analog, like the reference pictures (6:00:00 on
                // the 20th, the dot shown) and around them.
                renderer.selectFace("analog");
                if(!renderer.analogSeconds()) renderer.handle(hold);
                auto setTime=[&](int hour,int minute,int second) {
                    scene.localTime.tm_hour=hour; scene.localTime.tm_min=minute; scene.localTime.tm_sec=second;
                };
                scene=sampleData(); setTime(6,0,0); scene.localTime.tm_mday=20; shoot("analog",sm,scene);
                scene.batteryPercent=18;
                item(0,"02:40",appIcon(IconId::Stopwatch),StopwatchColors.background);
                item(1,"02:40",appIcon(IconId::Stopwatch),uint16_t(0xfd03));
                shoot("analog-info",sm,scene);
                scene=sampleData(); setTime(10,8,37); shoot("analog-seconds",sm,scene);
                setTime(3,15,0); scene.charging=true; item(0,"12:34",appIcon(IconId::Stopwatch),StopwatchColors.background);
                shoot("analog-over-date",sm,scene);
                renderer.handle(hold);
                scene=sampleData(); scene.timeValid=false; scene.batteryPercent=-1; scene.charging=true;
                shoot("analog-unknown",sm,scene);
                scene=sampleData(); setTime(6,32,0); scene.batteryPercent=100; scene.charging=true;
                item(0,"A very long label the row has to shorten",nullptr,std::nullopt);
                item(1,"計測中",appIcon(IconId::Settings),uint16_t(0x0000));
                shoot("analog-mixed",sm,scene);
                scene=sampleData(); item(0,"02:40",appIcon(IconId::Stopwatch),StopwatchColors.background);
                slide("analog-transition",0.45f);
                sm.screen=ScreenId::Home; sm.launcher.transition=0;
                // Work 11-3: Noonish at the references' 10:07 on the 20th, the
                // dot at the top, and around them.
                renderer.selectFace("noonish");
                if(!renderer.noonishSeconds()) renderer.handle(hold);
                scene=sampleData(); setTime(10,7,0); scene.localTime.tm_mday=20; shoot("noonish",sm,scene);
                scene.batteryPercent=18;
                item(0,"02:40",appIcon(IconId::Stopwatch),StopwatchColors.background);
                item(1,"02:40",appIcon(IconId::Stopwatch),uint16_t(0xfd03));
                shoot("noonish-info",sm,scene);
                setTime(12,0,0); shoot("noonish-together",sm,scene);
                setTime(6,0,15); shoot("noonish-opposite",sm,scene);
                setTime(7,30,0); scene.charging=true; shoot("noonish-row-boundary",sm,scene);
                scene=sampleData(); setTime(10,8,37); shoot("noonish-seconds",sm,scene);
                renderer.handle(hold);
                scene=sampleData(); scene.timeValid=false; scene.batteryPercent=-1; scene.charging=true;
                shoot("noonish-unknown",sm,scene);
                scene=sampleData(); setTime(4,40,0); scene.batteryPercent=100; scene.charging=true;
                item(0,"A very long label the row has to shorten",nullptr,std::nullopt);
                item(1,"計測中",appIcon(IconId::Settings),uint16_t(0x0000));
                shoot("noonish-mixed",sm,scene);
                scene=sampleData(); item(0,"02:40",appIcon(IconId::Stopwatch),StopwatchColors.background);
                slide("noonish-transition",0.45f);
                sm.screen=ScreenId::Home; sm.launcher.transition=0;
                renderer.selectFace("digital");
                // The timer's three views, as docs/Images/Timer draws them.
                sm.screen=ScreenId::Timer; sm.timer=TimerModel{};
                sm.timer.fields[1]=3; sm.timer.focus=1; shoot("timer-setup",sm,scene);
                sm.timer.focus=TimerFocusSet; shoot("timer-setup-set",sm,scene);
                sm.timer.view=TimerView::Countdown; sm.timer.seconds=161; shoot("timer-countdown",sm,scene);
                sm.timer.resetFill=500; shoot("timer-countdown-reset",sm,scene);
                sm.timer.resetFill=0; sm.timer.paused=true; shoot("timer-paused",sm,scene);
                sm.timer.view=TimerView::Ringing; sm.timer.seconds=1; shoot("timer-ringing",sm,scene);
                sm.screen=ScreenId::Home; sm.timer=TimerModel{};
            }
#endif
        }
    }
    heap_caps_free(incremental); heap_caps_free(reference);
    renderer.suppressStatsForTest(false);
    renderer.invalidate(); recording=true;
}
#endif
}
#endif
