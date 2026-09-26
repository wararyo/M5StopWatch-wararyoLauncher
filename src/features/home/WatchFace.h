#pragma once
#include "features/home/HomeInteraction.h"
#include "ui/graphics/Gfx.h"
#include "ui/rendering/Viewport.h"
#include "ui/rendering/PaintContext.h"
namespace launcher {
class WatchFace {
public:
    virtual ~WatchFace()=default;
    virtual const char* id() const=0;
    virtual bool begin(Gfx&,bool disableCache=false)=0;
    virtual void end()=0;
    // The environment is the one update() received. The face places itself:
    // how it moves while the list slides over it is its own choice, and only
    // the uncovered clip is the system's. A background whose extent or colour
    // changes is declared with FramePlan::damage, and so is the band of a
    // background other than the Renderer's black that the list's edge moved
    // over: the list's black changes nothing there of its own.
    virtual void plan(FramePlan&,Gfx&,const WatchEnvironment&,const WatchData&)=0;
    // Draws what reaches the context, its background first. The context is
    // already narrowed to the uncovered clip.
    virtual void paint(Gfx&,const PaintContext&)=0;
    // The face's own deadline (clock ticks and the like). Background labels
    // have their own deadlines; the runtime combines them with backgroundInterest.
    virtual TimeUs nextUpdate(TimeUs now,const WatchData& data) const=0;
    // A tap or long press on the resting clock. The face changes its own state
    // and may ask for the list; it never draws, reads hardware or saves here.
    virtual HomeOutcome handle(const HomeEvent&) { return {}; }
    // The frame's snapshot, before plan. Called once per frame; calling it
    // again with the same input must not move the face's state on.
    virtual void update(const WatchData&,const WatchEnvironment&,WatchChanges) {}
    // The background items the face shows, whose label deadlines the display
    // waits for. Items outside it never wake the display.
    virtual BackgroundInterest backgroundInterest(const BackgroundSnapshot&) const { return {}; }
    // The colour the app list is laid on over this face (RGB565). The list's
    // text keeps its own colours, so a face picks a background they read on.
    virtual uint16_t listBackground() const { return 0x0000; }
};
}
