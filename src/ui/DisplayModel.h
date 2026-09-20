#pragma once
#include "input/InputController.h"
#include <ctime>
namespace launcher {
enum class ScreenId { Home, AppList };
struct ScreenModel {
    ScreenId screen=ScreenId::Home;
    int width=468,height=468,selection=0;
    uint32_t homeCount=0;
    float transition=0,scroll=0;
    bool dragging=false,animating=false;
    const char* names[5]{}; // optional stable service/diagnostic labels
    const char* toast=nullptr;
};
struct WatchData {
    std::tm localTime{}; // JST, supplied by the service; no conversion in UI
    bool timeValid=false;
    int batteryPercent=-1;
    bool charging=false;
    TimeUs subsecondUs=0;
};
class DisplayDataSource {
public:
    virtual ~DisplayDataSource()=default;
    virtual WatchData sample(TimeUs) { return {}; }
    virtual TimeUs nextUpdate(TimeUs) const { return INT64_MAX; }
};
class RenderPort {
public:
    virtual ~RenderPort()=default;
    virtual void invalidate()=0;
    virtual void draw(const ScreenModel&,const WatchData&)=0;
    virtual TimeUs nextUpdate(TimeUs now,const WatchData& data) const=0;
};
inline TimeUs nextMinute(TimeUs now,const WatchData& data) {
    if (!data.timeValid || data.localTime.tm_sec<0 || data.localTime.tm_sec>59 ||
        data.subsecondUs<0 || data.subsecondUs>=1000000) return INT64_MAX;
    return now+(60-data.localTime.tm_sec)*1000000LL-data.subsecondUs;
}
}
