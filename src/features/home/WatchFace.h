#pragma once
#include "features/home/HomeInteraction.h"
#include "ui/graphics/Gfx.h"
#include "ui/rendering/Viewport.h"
#include "ui/rendering/Element.h"
namespace launcher {
class WatchFace {
public:
    virtual ~WatchFace()=default;
    virtual const char* id() const=0;
    virtual bool begin(Gfx&,bool disableCache=false)=0;
    virtual void end()=0;
    virtual void plan(FramePlan&,Gfx&,const DrawRegion&,const WatchData&)=0;
    virtual void paint(Gfx&,const FramePlan&)=0;
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
    // The colour the app list is laid on over this face (RGB565).
    virtual uint16_t listBackground() const { return 0x0000; }
};
}
