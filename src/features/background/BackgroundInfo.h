#pragma once
#include "core/AppId.h"
#include "core/Time.h"
#include "ui/graphics/IconBitmap.h"
#include <array>
#include <cstdint>
#include <optional>
namespace launcher {
// What an application running behind the screens asks the watch face to show
// (docs/task10/plan.md 4). The application formats the label and brings its
// own icon and, if it likes, a colour; the face sizes, colours, places and may
// leave them out, but never parses or reformats the label, and keeps no table
// from ids to pictures. No display, HAL or screen type appears here.
inline constexpr int BackgroundLabelBytes=48;   // UTF-8, terminator included
inline constexpr int BackgroundCapacity=4;      // providers, hence items
struct BackgroundInfo {
    TimeUs nextChangeAt=INT64_MAX;  // When the label changes; INT64_MAX: never.
    // A one-colour 8 bit coverage mask, or none. Only the reference is copied:
    // descriptor and pixels are static and unchanging while the app runs, and
    // another picture is another asset, never the same one rewritten.
    const IconBitmap* icon=nullptr;
    LaunchTargetId appId{};
    // RGB565, or none. Black is a colour like any other; whether to use it is
    // the face's choice.
    std::optional<uint16_t> suggestedColor{};
    char label[BackgroundLabelBytes]{};
};
// One frame's information, copied out of the providers so it cannot change
// while the frame is drawn. Items keep the providers' registration order.
struct BackgroundSnapshot {
    std::array<BackgroundInfo,BackgroundCapacity> items{};
    uint8_t count=0;
};
// The ids a watch face shows, whose deadlines are worth waking for.
struct BackgroundInterest {
    std::array<LaunchTargetId,BackgroundCapacity> ids{};
    uint8_t count=0;
};
// The first `shown` items, which is what both faces of task 10 display.
inline BackgroundInterest leadingItems(const BackgroundSnapshot& s,int shown) {
    BackgroundInterest interest;
    for (int i=0;i<s.count && i<shown && i<BackgroundCapacity;++i) interest.ids[interest.count++]=s.items[i].appId;
    return interest;
}
// The earliest label change among the items a face shows. Items it leaves out
// do not wake the display; a new item arrives through the change notification.
inline TimeUs nextChange(const BackgroundSnapshot& s,const BackgroundInterest& interest) {
    TimeUs due=INT64_MAX;
    for (int i=0;i<s.count;++i)
        for (int j=0;j<interest.count;++j)
            if (s.items[i].appId==interest.ids[j] && s.items[i].nextChangeAt<due) due=s.items[i].nextChangeAt;
    return due;
}
// An application's source of background information. It lives as long as the
// launcher, independent of whether its screen is open, and is asked only on the
// UI task. `out` arrives with the provider's id, an empty label, no icon, no
// colour and no deadline; returning false, or leaving the label empty, means
// nothing to show.
class BackgroundInfoProvider {
public:
    virtual ~BackgroundInfoProvider()=default;
    virtual LaunchTargetId id() const=0;
    virtual bool sample(TimeUs now,BackgroundInfo& out) const=0;
};
}
