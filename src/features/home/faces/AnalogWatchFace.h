#pragma once
#include "features/home/WatchFace.h"
#include "features/home/faces/AnalogLayout.h"
#include "features/home/faces/InfoRowView.h"
#include <array>
namespace launcher {
// Analog (docs/task11/plan.md): on the Renderer's black, the day of the month
// and the information row in grey, then white hour and minute hands, the hub,
// and an orange dot for the seconds that a long press shows or hides. Every
// part is an element with its own box and key, so a tick of the dot repaints
// the dot's old and new places only, and the hands move every ten seconds. The
// hands overlap the date and the row by design: whatever the damage touches is
// drawn again back to front. It stays still while the list covers it.
class AnalogWatchFace final : public WatchFace {
public:
    const char* id() const override { return "analog"; }
    const char* name() const override { return "Analog"; }
    const char* storageKey() const override { return "wf_analog"; }
    void bindPreferences(FacePreferences* prefs) override { control_.bindPreferences(prefs); }
    bool begin(Gfx&,bool disableCache=false) override;
    void end() override;
    void update(const WatchData& d,const WatchEnvironment&,WatchChanges) override { control_.update(d); }
    void plan(FramePlan&,Gfx&,const WatchEnvironment&,const WatchData&) override;
    void paint(Gfx&,const PaintContext&) override;
    TimeUs nextUpdate(TimeUs now,const WatchData& data) const override { return control_.nextUpdate(now,data); }
    HomeOutcome handle(const HomeEvent& e) override { return control_.handle(e); }
    BackgroundInterest backgroundInterest(const BackgroundSnapshot& s) const override { return control_.backgroundInterest(s); }
    bool seconds() const { return control_.seconds(); }
private:
    // Back to front.
    enum Part { Date,Battery,Item0,Item1,Hour,Minute,Hub,Second,PartCount };
    AnalogControl control_;
    InfoRowView row_;
    InfoRowView::Font date_{};
    AnalogLayout layout_{};
    AnalogTime time_{};
    char dateText_[3]{};
    AnalogDatePlace datePlace_{};
    AnalogStroke strokes_[PartCount]{};    // Hour, Minute, Hub, Second
    std::array<Element,PartCount> elements_{};
    std::array<Rect,PartCount> boxes_{};
    Viewport viewport_{};
};
}
