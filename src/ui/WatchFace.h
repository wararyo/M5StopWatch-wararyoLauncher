#pragma once
#include "features/home/HomeModel.h"
#include "ui/graphics/Gfx.h"
#include "ui/Viewport.h"
#include "Element.h"
namespace launcher {
class WatchFace {
public:
    virtual ~WatchFace()=default;
    virtual const char* id() const=0;
    virtual bool begin(Gfx&,bool disableCache=false)=0;
    virtual void end()=0;
    virtual void plan(FramePlan&,Gfx&,const DrawRegion&,const WatchData&)=0;
    virtual void paint(Gfx&,const FramePlan&)=0;
    virtual TimeUs nextUpdate(TimeUs now,const WatchData& data) const=0;
};
}
