#pragma once
#include "host/LaunchRegistry.h"
#include "features/launcher/AppListModel.h"
#include "input/InputController.h"
#include "ui/list/ListController.h"
#include <algorithm>
#include <array>
namespace launcher {
// What one launcher input asks of the app. The launcher never opens a screen
// itself: it names the decided entry, and ScreenManager enters it. A null
// target is a row the registry does not know; the app answers it as it
// answers an entry that cannot open.
struct LauncherOutcome {
    bool changed=false,open=false;
    const LaunchEntry* target=nullptr;
};
// The clock and the app list: the slide between them, who owns a drag, and
// the launcher's own list controller. Scrolling and deciding rows are the
// shared ListController's; only the launcher's meaning of them lives here,
// such as a pull down from the top of the list going back to the clock.
// Timing is by elapsed time, never by the number of frames.
class LauncherController {
public:
    explicit LauncherController(Viewport viewport={});
    // The list borrows rows_, so a copy would point into the original.
    LauncherController(const LauncherController&)=delete;
    LauncherController& operator=(const LauncherController&)=delete;
    LauncherOutcome handle(const Events& e,TimeUs now);
    // Advances the slide and the list; true when either moved.
    bool update(TimeUs now);
    TimeUs nextUpdate() const { return std::min(transitionFrame_,list_.nextUpdate()); }
    bool active() const { return drag_!=Drag::None || transitionAnimating_ || list_.active(); }
    // The list rather than the clock is what the launcher shows (or is sliding to).
    bool listShown() const { return listShown_; }
    bool transitioning() const { return transition_>0 && transition_<1; }
    // The clock alone, still and untouched: the only time the watch face gets
    // taps and long presses.
    bool atRest() const { return !listShown_ && transition_==0 && !transitionAnimating_ && drag_==Drag::None; }
    // The slide up to the list, as A/B start it from the clock. Asked for by
    // a watch face (HomeRequest::OpenAppList); false if the list is already
    // shown or on its way, so a repeated request does not restart it.
    bool openList(TimeUs now);
    bool scrolling() const { return list_.active(); }
    // An app opens over the list: whatever was still moving arrives now, so
    // the hidden launcher neither draws frames nor shows under the screen. The
    // row and the scroll stay for the return.
    void suspend();
    // The home key: the clock, with the list back at its top.
    void home();
    // The list's state and the slide. Names and dimming are the app's to add
    // (applySlots), since the slot catalog is not the launcher's.
    AppListModel model() const;
private:
    void animateTransition(float transition,TimeUs now);
    void stopTransition() { transitionAnimating_=false; transitionFrame_=INT64_MAX; }
    Viewport viewport_{};
    bool listShown_=false;
    // Home to list, 0 to 1.
    float transition_=0;
    ListController list_;
    std::array<ListRow,AppListCount> rows_{};
    // Who owns the current drag, fixed when it starts: the clock's pull up,
    // the list's pull back home from its top, or the list's own scrolling.
    enum class Drag { None,Watch,List,Return } drag_=Drag::None;
    bool transitionAnimating_=false;
    float fromTransition_=0,toTransition_=0,dragTransition_=0;
    TimeUs transitionStart_=0,transitionFrame_=INT64_MAX;
};
}
