#pragma once
#include "app/FrameModel.h"
namespace launcher {
class RenderPort {
public:
    virtual ~RenderPort()=default;
    virtual void invalidate()=0;
    virtual void draw(const ScreenModel&,const WatchData&)=0;
    virtual TimeUs nextUpdate(TimeUs now,const WatchData& data) const=0;
};
}
