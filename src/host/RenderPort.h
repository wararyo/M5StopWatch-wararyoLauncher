#pragma once
#include "host/FrameModel.h"
#include "features/home/HomeModel.h"
namespace launcher {
class RenderPort {
public:
    virtual ~RenderPort()=default;
    virtual void invalidate()=0;
    virtual void draw(const FrameModel&,const WatchData&)=0;
    // The clock face's own deadline. The runtime asks only while the clock is
    // on screen (clockVisible in host/FrameComposer.h).
    virtual TimeUs nextUpdate(TimeUs now,const WatchData& data) const=0;
    // The background items the clock shows; the runtime also waits for their
    // label deadlines, under the same visibility rule.
    virtual BackgroundInterest backgroundInterest(const WatchData&) const { return {}; }
};
}
