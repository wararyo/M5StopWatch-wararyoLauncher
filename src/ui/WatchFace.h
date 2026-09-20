#pragma once
#include <M5GFX.h>
#include "DisplayModel.h"
#include "Element.h"
namespace launcher {
using Gfx=m5gfx::LovyanGFX;
class WatchFace {
public:
    virtual ~WatchFace()=default;
    virtual const char* id() const=0;
    virtual bool begin(Gfx&,bool disableCache=false)=0;
    virtual void end()=0;
    virtual void plan(FramePlan&,Gfx&,const ScreenModel&,const WatchData&)=0;
    virtual void paint(Gfx&,const FramePlan&)=0;
    virtual TimeUs nextUpdate(TimeUs now,const WatchData& data) const=0;
};
}
