#pragma once
#include "features/home/faces/HandsFace.h"
#include "features/home/faces/NoonishBackground.h"
namespace launcher {
// Noonish (docs/task11/plan.md 2.4, plan-11-3.md): HandsFace over four
// regions split by the lines of the hands, the date and the row in a pale
// ink, and a dot that lightens whatever region lies under each of its pixels.
//
// The background covers the whole face, so the Renderer's black is never
// restored under it: it is painted inside every frame's area, and whatever
// changes it is declared. When the hands step the regions turn with them, and
// all of the face that shows is damaged; so is the band the list's edge moved
// over, as on Forest. A tick of the dot alone leaves the regions as they are.
class NoonishWatchFace final : public HandsFace {
public:
    NoonishWatchFace():HandsFace(NoonishDateX,NoonishInk) {}
    const char* id() const override { return "noonish"; }
    const char* name() const override { return "Noonish"; }
    const char* storageKey() const override { return "wf_noonish"; }
    void end() override;
    Rect opaqueArea() const override { return clip_; }
    // The layout is the last frame's: the clock layer plans every frame,
    // covered or not, before anything shows the band.
    uint16_t homeGestureBackground(const WatchData& d) const override { return noonishTopColour(layout(),analogTime(d)); }
private:
    class Regions final : public Backdrop {
    public:
        uint16_t at(int x,int y) const override { return noonishColour(split,x,y); }
        NoonishSplit split{};
    };
    void planBackground(FramePlan&,const WatchEnvironment&) override;
    void paintBackground(Gfx&,const PaintContext&) override;
    const Backdrop& backdrop() const override { return regions_; }
    void paintDot(Gfx& g,const AnalogStroke& s) override;
    Regions regions_;
    Rect clip_{},shownClip_{};
    int shownStep_=0;
    Viewport shownViewport_{};
    bool planned_=false;
};
}
