#pragma once
#include "features/home/HomeModel.h"
#include "features/background/BackgroundInfoHub.h"
#include "ui/rendering/Viewport.h"
#include <cstring>
namespace launcher {
// What reaches the watch face from the system, and what it may ask back
// (docs/task10/plan-10-2.md 2). No display type appears here, so the screen
// control and the tests use it without M5GFX.
//
// Only a clock at rest receives these: the system recognises the gestures and
// keeps the swipe up, the list's own input, A/B and home. Coordinates are the
// clock's own, with its origin at the top left of the resting viewport.
enum class HomeEventKind : uint8_t { Tap,LongPress };
struct HomeEvent {
    HomeEventKind kind=HomeEventKind::Tap;
    int x=0,y=0;
    TimeUs at=0;
};
// The one screen request a face can make in task 10. The system carries it
// out with the same slide as the buttons; a face never drives screens.
enum class HomeRequest : uint8_t { None,OpenAppList };
struct HomeOutcome {
    bool changed=false;       // The face's own state changed: draw again.
    HomeRequest request=HomeRequest::None;
    // The face kept a change it could not store (docs/task10/plan-10-5.md 4).
    // It still shows it; the system says the save failed.
    bool saveFailed=false;
};
// A face as settings lists it: no display types, no face class.
struct WatchFaceChoice {
    const char* id=nullptr;    // stable: what is stored, never a position
    const char* name=nullptr;  // what the list shows
};
enum class FaceChoiceResult : uint8_t {
    Selected,    // Shown now and stored.
    Unchanged,   // Already shown and stored: nothing was done.
    SaveFailed,  // Shown now, but the next start will show the last stored face.
    Failed,      // Could not be shown; the previous face is back.
    Unknown,     // No such face.
};
// The boundary between screen control and the clock layer. The renderer
// implements it by handing the event to the selected face; settings chooses
// the face through it (docs/task10/plan-10-5.md 5).
class HomeControlPort {
public:
    virtual ~HomeControlPort()=default;
    virtual HomeOutcome handle(const HomeEvent& event)=0;
    virtual int faceCount() const { return 0; }
    virtual WatchFaceChoice faceAt(int) const { return {}; }
    // Index of the face shown, -1 without one.
    virtual int currentFace() const { return -1; }
    // Begins the face and stores the choice. Only a confirmed choice comes
    // here: moving the cursor over a face does not.
    virtual FaceChoiceResult chooseFace(const char*) { return FaceChoiceResult::Unknown; }
};
// Where the face is shown this frame. `listProgress` is the app list's slide
// over it, 0 (clock) to 1 (list); the clip is what the system leaves uncovered.
struct WatchEnvironment {
    Viewport viewport{};
    Rect clip{};
    float listProgress=0;
    bool visible() const { return !clip.empty(); }
};
// Why a frame differs from the one before, as far as a face is concerned.
// Faces still read the whole snapshot; the bits only save them the comparing.
enum WatchChange : uint16_t {
    WatchTime=1,        // The clock reading or its validity.
    WatchBattery=2,     // Percent or charging.
    WatchBackground=4,  // Background items added, removed, relabelled, restyled.
    WatchProgress=8,    // The list's slide over the clock.
    WatchResumed=16,    // First frame after a full repaint (a wake, say).
    WatchSelected=32,   // First frame of a newly selected face.
};
using WatchChanges=uint16_t;
inline bool sameBackground(const BackgroundSnapshot& a,const BackgroundSnapshot& b) {
    if (a.count!=b.count) return false;
    for (int i=0;i<a.count;++i)
        if (a.items[i].appId!=b.items[i].appId || std::strcmp(a.items[i].label,b.items[i].label)!=0 ||
            !sameStyle(a.items[i],b.items[i])) return false;
    return true;
}
// The data half of the changes; progress, resume and selection are the clock
// layer's to add. A deadline moving on is no change.
inline WatchChanges watchChanges(const WatchData& before,const WatchData& after) {
    WatchChanges c=0;
    const auto& x=before.localTime; const auto& y=after.localTime;
    if (before.timeValid!=after.timeValid || x.tm_sec!=y.tm_sec || x.tm_min!=y.tm_min ||
        x.tm_hour!=y.tm_hour || x.tm_mday!=y.tm_mday || x.tm_mon!=y.tm_mon || x.tm_year!=y.tm_year)
        c|=WatchTime;
    if (before.batteryPercent!=after.batteryPercent || before.charging!=after.charging ||
        before.chargingKnown!=after.chargingKnown) c|=WatchBattery;
    if (!sameBackground(before.background,after.background)) c|=WatchBackground;
    return c;
}
}
