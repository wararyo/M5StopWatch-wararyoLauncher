#include "RenderDiagnostics.h"
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
#include "IconSet.h"
#include "Renderer.h"
#include "Text.h"
#include "VlwFont.h"
#include "app/AppRegistry.h"
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace launcher {
namespace {
bool recording=false;
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
Distribution drawTime[3],interval[3],inputLatency,wakeLatency;
TimeUs lastEnd=0,windowStart=0,inputAt=-1,wakeAt=-1;
int lastMode=-1;
uint32_t lastLayouts=0,lastPaints=0;
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
    void plan(FramePlan& f,Gfx&,const ScreenModel&,const WatchData& d) override {
        ha_=f.add(a_,{40,100,170,100},hashValue(d.localTime.tm_min));
        hb_=f.add(b_,{130,120,170,100},1);
    }
    void paint(Gfx& g,const FramePlan& f) override {
        if(f.shouldPaint(ha_)) g.fillRect(40,100,170,100,0x1234);
        if(f.shouldPaint(hb_)) g.fillRect(130,120,170,100,0x5678);
    }
    TimeUs nextUpdate(TimeUs,const WatchData&) const override { return INT64_MAX; }
};
}
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
void recordInput(TimeUs now) { if(recording && inputAt<0) inputAt=now; }
void recordWake(TimeUs now) { if(recording) wakeAt=now; }
void recordRender(const ScreenModel& m,TimeUs start,TimeUs end,bool painted,uint32_t) {
    if(!recording) return;
    if(!painted) { inputAt=-1; return; }
    const int mode=m.transition>0 && m.transition<1 ? 0 : m.dragging || m.animating ? 1 : 2;
    drawTime[mode].add(end-start);
    if(mode<2 && lastMode==mode && lastEnd) interval[mode].add(end-lastEnd);
    lastMode=mode; lastEnd=end;
    if(inputAt>=0) { inputLatency.add(end-inputAt); inputAt=-1; }
    if(wakeAt>=0) { wakeLatency.add(end-wakeAt); wakeAt=-1; }
}
void reportRenderDiagnostics(const Renderer& renderer,TimeUs now) {
    if(!recording) return;
    if(!windowStart) { windowStart=now; lastLayouts=renderer.layouts(); lastPaints=renderer.paints(); return; }
    if(now-windowStart<60000000) return;
    const char* drawNames[]={"transition-draw","scroll-draw","single-draw"};
    const char* gapNames[]={"transition-interval","scroll-interval"};
    for(int i=0;i<3;++i) drawTime[i].print(drawNames[i]);
    for(int i=0;i<2;++i) {
        interval[i].print(gapNames[i]);
        std::printf("[RenderDiag] %s fps=%.2f (continuous pairs; keep moving)\n",gapNames[i],
            interval[i].total ? interval[i].count*1000000.0/interval[i].total : 0.0);
    }
    inputLatency.print("sampled-input-to-end"); wakeLatency.print("wake-request-to-end");
    std::printf("[RenderDiag] window_us=%lld layouts=%u paints=%u stack_free=%u internal_free=%u internal_largest=%u psram_free=%u psram_largest=%u\n",
        static_cast<long long>(now-windowStart),unsigned(renderer.layouts()-lastLayouts),unsigned(renderer.paints()-lastPaints),
        unsigned(uxTaskGetStackHighWaterMark(nullptr)),unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
        unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
        unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
    for(auto& d:drawTime) d={};
    for(auto& d:interval) d={};
    inputLatency={}; wakeLatency={}; lastMode=-1; lastEnd=0;
    windowStart=now; lastLayouts=renderer.layouts(); lastPaints=renderer.paints();
}
void runRepaintCheck(Renderer& renderer,M5GFX& display,const SlotCatalog& catalog) {
    recording=false;
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
                                  "バージョン","スロット","エラー","起動できませんでした"}) covered(text);
            char fitted[128];
            char small[2]; fitText(display,"a",small,sizeof(small),4096); ++checks;
            if(std::strcmp(small,"a")!=0) { ++failures; std::printf("[Verify] FAIL bounded text capacity\n"); }
            fitText(display,"外部アプリ😀",fitted,sizeof(fitted),4096); ++checks;
            if(std::strcmp(fitted,"外部アプリ?")!=0) { ++failures; std::printf("[Verify] FAIL unsupported glyph fallback\n"); }
            // Every row needs its mask, and the mask must stay inside the
            // smaller unselected circle: pushGrayscaleImage paints the whole
            // rectangle, so an overhang would show as a square corner.
            {
                // transition=1: row 0 sits at the centre with its full radius.
                ScreenModel probe; probe.width=w; probe.height=h; probe.transition=1;
                const float iconScale=float(std::min(w,h))/468;
                const int radius=layoutRow(probe,0).radius-selectionGrowth(probe);
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
            auto check=[&](const char* name,ScreenModel m,WatchData d) {
                renderer.draw(m,d); display.readRect(0,0,w,h,incremental);
                renderer.invalidate(); renderer.draw(m,d); display.readRect(0,0,w,h,reference);
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
            ScreenModel m; m.width=w; m.height=h; auto d=sampleData();
            renderer.invalidate();
            // Preserve the original 24 sweeps, expanded to all five rows.
            for(int variant=0;variant<24;++variant) {
                m.transition=float(variant%21)/20;
                for(int i=0;i<=17+variant*5;++i) {
                    m.scroll=float((i*3)%(4*rowSpacing(m)+1)); m.selection=(i/11)%5;
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
                m.selection=i; m.scroll=i*rowSpacing(m);
                check("five-rows",m,d);
                m.toast="準備中"; check("toast-on",m,d);
                m.scroll+=8; check("toast-overlap",m,d);
                m.toast=nullptr; check("toast-off",m,d);
            }
            m.names[2]="非常に長い外部アプリ名と未収録文字😀";
            m.scroll=2*rowSpacing(m); check("long-japanese",m,d);
            // Settings covers the list rather than sliding it away, so the rows
            // it hides have to be erased by the same differential plan.
            m.names[2]=nullptr; m.screen=ScreenId::Settings; m.settings=SettingsModel{};
            m.settings.lines[0]="wararyoLauncher";
            m.settings.lines[1]="0.0.0-verify"; m.settings.lines[2]="5.5.0";
            for(int row=0;row<SettingsMenuRows;++row) {
                m.settings.cursor=row; check("settings-menu",m,d);
            }
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
            check("settings-left",m,d);
            // A row that only changed colour still has to repaint, so the dim
            // flag has to reach the fingerprint.
            m.scroll=2*rowSpacing(m); m.selection=2;
            for(int i=2;i<5;++i) m.rowDimmed[i]=true;
            check("list-dimmed",m,d);
            m.names[2]=catalog.slots[0].name; m.rowDimmed[2]=false;
            check("list-named",m,d);
            m.names[2]=nullptr;
            for(int i=0;i<5;++i) m.rowDimmed[i]=false;
            // The external detail: every state a slot can report, then the two
            // phases the boot commit adds. The injected catalog is what makes
            // a corrupt slot reachable without breaking a real one.
            m.screen=ScreenId::External;
            for(int slot=1;slot<=SlotCount;++slot) {
                const auto& entry=catalog.slots[slot-1];
                m.external=ExternalModel{}; m.external.slot=slot; m.external.status=entry.status;
                m.external.name=entry.name[0] ? entry.name : nullptr;
                m.external.version=entry.version[0] ? entry.version : nullptr;
                m.external.error=entry.error;
                for(int cursor=0;cursor<externalButtonCount(m.external);++cursor) {
                    m.external.cursor=cursor; check("external-browse",m,d);
                }
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
            m.external.phase=ExternalPhase::BootFailed; m.external.cursor=0;
            m.external.message="ESP_ERR_IMAGE_INVALID"; check("external-failed",m,d);
            // A guest chooses its own name, so the detail has to survive one
            // that is too long and holds a glyph the subset does not carry.
            m.external=ExternalModel{}; m.external.slot=1; m.external.status=SlotStatus::Ready;
            m.external.name="非常に長い外部アプリ名と未収録文字😀";
            m.external.version="1.0.0-verify"; check("external-long-name",m,d);
            m.screen=ScreenId::AppList; m.external=ExternalModel{};
            check("external-left",m,d);
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
            std::printf("[Verify] checks=%u mismatches=%u result=%s\n",checks,failures,failures ? "FAIL" : "PASS");
        }
    }
    heap_caps_free(incremental); heap_caps_free(reference);
    renderer.invalidate(); recording=true;
}
}
#endif
