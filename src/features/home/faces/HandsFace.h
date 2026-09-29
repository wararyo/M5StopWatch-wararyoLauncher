#pragma once
#include "features/home/WatchFace.h"
#include "features/home/faces/AnalogLayout.h"
#include "features/home/faces/Backdrop.h"
#include "features/home/faces/InfoRowView.h"
#include <array>
namespace launcher {
// What Analog and Noonish share (docs/task11/plan.md 3): the day of the month
// and the information row, then the hour and minute hands, the hub, and a dot
// for the seconds that a long press shows or hides. Every part is an element
// with its own box and key, so a tick of the dot repaints the dot's old and new
// places only, and the hands move every ten seconds. The hands overlap the
// date and the row by design: whatever the damage touches is drawn again back
// to front. The face stays still while the list covers it.
//
// Each face owns its background, its colours and how the dot is drawn: the
// hooks below. Its control, and so its record, is its own.
class HandsFace : public WatchFace {
public:
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
protected:
    // Back to front.
    enum Part { Date,Battery,Item0,Item1,Hour,Minute,Hub,Second,PartCount };
    // The date's reference position (AnalogDateX, NoonishDateX) and the ink of
    // the date and the row.
    HandsFace(float dateX,uint16_t ink):dateX_(dateX),ink_(ink) {}
    // After the parts are placed: the face declares what its background
    // changes (FramePlan::damage).
    virtual void planBackground(FramePlan&,const WatchEnvironment&) {}
    // Before the parts, inside the frame's area.
    virtual void paintBackground(Gfx&,const PaintContext&) {}
    virtual const Backdrop& backdrop() const=0;
    virtual void paintDot(Gfx&,const AnalogStroke&)=0;
    const AnalogLayout& layout() const { return layout_; }
    const AnalogTime& time() const { return time_; }
private:
    const float dateX_;
    const uint16_t ink_;
    AnalogControl control_;
    InfoRowView row_;
    InfoRowView::Font date_{};
    AnalogLayout layout_{};
    AnalogTime time_{};
    char dateText_[3]{};
    AnalogDatePlace datePlace_{};
    std::array<AnalogStroke,PartCount> strokes_{};  // Hour, Minute, Hub, Second
    std::array<Element,PartCount> elements_{};
    std::array<Rect,PartCount> boxes_{};
};
}
