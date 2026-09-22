#pragma once
#include "WatchFace.h"
#include "SettingsLayout.h"
namespace launcher {
// The settings layer. It plans nothing at all on the clock and the app list, so
// the frame cost is only paid while settings is open.
class SettingsLayer {
public:
    void plan(FramePlan& frame,Gfx& g,const ScreenModel& m,const lgfx::IFont* font);
    void paint(Gfx& g,const FramePlan& frame,const ScreenModel& m,const lgfx::IFont* font);
private:
    // The busiest view is the date editor: title, five fields, three
    // separators and two buttons. Information needs six: title, three lines,
    // one action and one button.
    static constexpr int Capacity=11;
    enum Kind { Title,MenuRow,Field,Separator,Button,InfoLine,Action };
    struct Item {
        Rect box{};
        Kind kind=Title;
        int index=0,labelX=0,centerY=0;
        bool selected=false,editing=false;
        // An action already taken. Its text does not change, so this has to
        // reach the fingerprint or the colour change would not repaint.
        bool done=false;
        char text[40]{};
    };
    void build(const ScreenModel& m);
    Item items_[Capacity]{};
    Element elements_[Capacity]{};
    int handles_[Capacity]{};
    int count_=0;
};
}
