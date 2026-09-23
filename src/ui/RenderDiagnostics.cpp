#include "RenderDiagnostics.h"
#ifdef LAUNCHER_RENDER_METRICS
#include "Renderer.h"
#include "storage/Settings.h"
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include <cstdio>
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
#include "features/launcher/AppIcons.h"
#include "Text.h"
#include "VlwFont.h"
#include "app/AppRegistry.h"
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
class TestFace final : public WatchFace {
    Element a_,b_; int ha_=-1,hb_=-1;
public:
    const char* id() const override { return "test-overlap"; }
    bool begin(Gfx&,bool) override { a_={}; b_={}; return true; }
    void end() override {}
    void plan(FramePlan& f,Gfx&,const DrawRegion&,const WatchData& d) override {
        ha_=f.add(a_,{40,100,170,100},hashValue(d.localTime.tm_min));
        hb_=f.add(b_,{130,120,170,100},1);
    }
    void paint(Gfx& g,const FramePlan& f) override {
        if(f.shouldPaint(ha_)) g.fillRect(40,100,170,100,0x1234);
        if(f.shouldPaint(hb_)) g.fillRect(130,120,170,100,0x5678);
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
void recordRender(const ScreenModel& m,TimeUs start,TimeUs end,bool painted,uint32_t) {
    if(!recording) return;
    if(!painted) { inputAt=-1; return; }
    const int mode=modeOf(m.activity);
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
void reportRenderDiagnostics(const Renderer& renderer,TimeUs now) {
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
void runRepaintCheck(Renderer& renderer,M5GFX& display,const SlotCatalog& catalog) {
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
            for(const auto& entry:AppRegistry) covered(entry.name);
            // Every fixed string the launcher can put on screen, so a font
            // subset that missed one fails here rather than on the device.
            for(const char* text:{"準備中","日時","輝度","消灯時間","情報","戻る","保存","キャンセル",
                                  "30秒","時刻を保存しました","保存しました","日付が正しくありません",
                                  "保存に失敗しました","時計を設定できません",
                                  "検証中","空き","破損","読み取り失敗","非対応","起動","起動中",
                                  "バージョン","スロット","エラー","起動できませんでした",
                                  "統計情報を表示"}) covered(text);
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
                for(const auto& entry:AppRegistry) {
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
            auto check=[&](const char* name,ScreenModel m,WatchData d) {
                composeHomeRegion(m);
                renderer.draw(m,d); display.readRect(0,0,w,h,incremental);
                renderer.invalidate(); renderer.draw(m,d); display.readRect(0,0,w,h,reference);
                compare(name);
            };
            // The list's cached name images against the same names drawn as
            // glyphs, both on a full repaint: the cache has to be invisible.
            auto& view=renderer.listViewForTest();
            auto& settingsLayer=renderer.settingsForTest();
            auto directWith=[&](ListView& list,const char* name,ScreenModel m,WatchData d) {
                composeHomeRegion(m);
                renderer.invalidate(); renderer.draw(m,d); display.readRect(0,0,w,h,incremental);
                list.textImagesForTest(false);
                renderer.invalidate(); renderer.draw(m,d); display.readRect(0,0,w,h,reference);
                list.textImagesForTest(true);
                compare(name);
            };
            auto direct=[&](const char* name,ScreenModel m,WatchData d) { directWith(view,name,m,d); };
            ScreenModel m; m.width=w; m.height=h; auto d=sampleData();
            renderer.invalidate();
            // Preserve the original 24 sweeps, expanded to all five rows.
            for(int variant=0;variant<24;++variant) {
                m.transition=float(variant%21)/20;
                for(int i=0;i<=17+variant*5;++i) {
                    m.list.scroll=float((i*3)%(4*rowSpacing(m.viewport())+1)); m.list.selection=(i/11)%5;
                    composeHomeRegion(m);
                    renderer.draw(m,d);
                    if(i%8==0) vTaskDelay(1);
                }
                check("original-24",m,d);
            }
            for(int direction: {1,-1}) for(int i=0;i<=20;++i) {
                m.transition=float(direction==1 ? i : 20-i)/20;
                check("roundtrip",m,d); vTaskDelay(1);
            }
            m.screen=ScreenId::AppList; m.transition=1;
            for(int i=0;i<5;++i) {
                m.list.selection=i; m.list.scroll=i*rowSpacing(m.viewport());
                check("five-rows",m,d);
                m.toast="準備中"; check("toast-on",m,d);
                m.list.scroll+=8; check("toast-overlap",m,d);
                m.toast=nullptr; check("toast-off",m,d);
            }
            m.names[2]="非常に長い外部アプリ名と未収録文字😀";
            m.list.scroll=2*rowSpacing(m.viewport()); check("long-japanese",m,d);
            // Settings covers the list rather than sliding it away, so the rows
            // it hides have to be erased by the same differential plan.
            m.names[2]=nullptr; m.screen=ScreenId::Settings; m.settings=SettingsModel{};
            m.settings.savedBrightness=Settings{}.brightness;
            m.settings.savedScreenOffSec=Settings{}.screenOffSec;
            m.settings.lines[0]="wararyoLauncher";
            m.settings.lines[1]="0.0.0-verify"; m.settings.lines[2]="5.5.0";
            // The menu is the shared list without icons (docs/task9/plan-9-3.md):
            // every row selected where A leaves it, then scroll positions
            // between rows, as a drag or the inertia leaves them.
            const int spacing=rowSpacing(m.viewport());
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
                m.settings.view=view; m.settings.editing=false;
                m.settings.fields[0]=view==SettingsView::DateTime ? 2026 :
                    view==SettingsView::Brightness ? 90 : 1;
                m.settings.fields[1]=9; m.settings.fields[2]=21;
                m.settings.fields[3]=23; m.settings.fields[4]=59;
                for(int slot=0;slot<settingsSlotCount(view);++slot) {
                    m.settings.cursor=slot; check("settings-slot",m,d);
                    if(slot<settingsFieldCount(view)) {
                        m.settings.editing=true; check("settings-editing",m,d);
                        m.settings.editing=false;
                    }
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
            m.list.scroll=2*rowSpacing(m.viewport()); m.list.selection=2;
            for(int i=2;i<5;++i) m.rowDimmed[i]=true;
            check("list-dimmed",m,d);
            m.names[2]=catalog.slots[0].name; m.rowDimmed[2]=false;
            check("list-named",m,d);
            // The shared list's caches (docs/task9/plan-9-2.md 9-2e).
            for(int i=0;i<5;++i) {
                m.list.selection=i; m.list.scroll=i*rowSpacing(m.viewport());
                direct("list-image",m,d);
            }
            direct("list-image-dimmed-named",m,d);
            m.names[2]="非常に長い外部アプリ名と未収録文字😀"; direct("list-image-long",m,d);
            m.names[2]=""; check("list-empty-name",m,d); direct("list-image-empty",m,d);
            // New content under the same id, then the same name under two ids.
            m.names[2]="外部アプリA"; check("list-content",m,d);
            m.names[2]="外部アプリB"; check("list-content-change",m,d); direct("list-image-content",m,d);
            m.names[2]=m.names[3]="同じ名前"; m.rowDimmed[3]=false;
            check("list-same-name",m,d); direct("list-image-same-name",m,d);
            m.names[3]=nullptr; m.rowDimmed[3]=true;
            // Moving rows must not shorten or render their names again: one
            // sweep to warm every slot, then a second that may not add any.
            {
                ScreenModel sweep=m; sweep.list.selection=2;
                for(int pass=0;pass<2;++pass) {
                    const auto before=view.cacheStats();
                    for(int y=0;y<=4*rowSpacing(sweep.viewport());y+=6) {
                        sweep.list.scroll=float(y); composeHomeRegion(sweep); renderer.draw(sweep,d);
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
                m.transition=float(step)/20;
                for(int y: {0,50,2*rowSpacing(m.viewport()),4*rowSpacing(m.viewport())}) {
                    m.list.scroll=float(y); check("list-slot-reuse",m,d);
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
                renderer.invalidate(); composeHomeRegion(m); renderer.draw(m,d);
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
            m.names[2]=nullptr;
            for(int i=0;i<5;++i) m.rowDimmed[i]=false;
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
            m={}; m.width=w; m.height=h; check("home",m,d);
            d.localTime.tm_min=42; check("minute",m,d);
            d.localTime.tm_mday=20; d.localTime.tm_wday=0; check("date",m,d);
            d.timeValid=false; d.batteryPercent=-1; check("unknown",m,d);
            d=sampleData(); d.charging=true; check("charging",m,d);
            display.fillScreen(0x1234); renderer.invalidate(); check("wake-invalidate",m,d);
            renderer.selectFace("digital",true); check("cache-disabled",m,d);
            m.transition=0.45f; check("cache-disabled-transition",m,d);
            renderer.capacityForTest(2); check("capacity-overflow",m,d);
            renderer.capacityForTest(FramePlan::Capacity); check("capacity-recovery",m,d);
            static TestFace alternate;
            renderer.registerFace(alternate); renderer.selectFace(alternate.id());
            check("alternate-face",m,d); ++d.localTime.tm_min; check("overlap-foreground",m,d);
            renderer.selectFace("digital"); m.transition=0; check("digital-restored",m,d);
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
        }
    }
    heap_caps_free(incremental); heap_caps_free(reference);
    renderer.suppressStatsForTest(false);
    renderer.invalidate(); recording=true;
}
#endif
}
#endif
