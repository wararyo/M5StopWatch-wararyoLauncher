#pragma once
#include "features/background/BackgroundInfo.h"
namespace launcher {
// What changed between two collections: ids, labels, icon references and
// suggested colours (set or not, and which). A deadline that moved on while all
// of them stayed is no change. Items keep the registration order, so the same
// set of ids is always in the same order.
enum BackgroundChange : uint8_t {
    BackgroundUnchanged=0,BackgroundAdded=1,BackgroundRemoved=2,BackgroundRelabeled=4,
    BackgroundRestyled=8  // Icon or suggested colour.
};
// Gathers the registered providers into one owned, fixed-size snapshot
// (docs/task10/plan-10-1.md 2). Providers are registered once at start-up and
// must outlive the hub; nothing is allocated after that and nothing polls on
// its own: the runtime collects when it draws a visible clock.
class BackgroundInfoHub {
public:
    enum class AddResult : uint8_t { Added,Duplicate,Full };
    // A second provider for the same id, or one past the capacity, is refused
    // and counted, so the diagnostics can report it.
    AddResult add(const BackgroundInfoProvider& provider);
    // Samples every provider in registration order into the snapshot. The
    // labels are copied, terminated and cut back to whole UTF-8 characters,
    // and an empty one drops its item. An icon without pixels or with no area
    // becomes no icon. Clears the pending notification.
    uint8_t collect(TimeUs now);
    const BackgroundSnapshot& snapshot() const { return snapshot_; }
    // For a provider whose state changes away from a screen the runtime is
    // already redrawing (a future timer's expiry, say). UI task only. It only
    // marks the information stale: a blank panel or a covered clock redraws
    // nothing for it and the next visible frame collects anyway.
    void invalidate() { pending_=true; }
    bool pending() const { return pending_; }
    int providers() const { return providerCount_; }
    int rejected() const { return rejected_; }
private:
    std::array<const BackgroundInfoProvider*,BackgroundCapacity> providers_{};
    uint8_t providerCount_=0,rejected_=0;
    // The next snapshot is built beside the current one, off the UI stack.
    BackgroundSnapshot snapshot_{},scratch_{};
    bool pending_=false;
};
// Terminates `label` within its buffer and drops a trailing partial UTF-8
// character; returns the resulting length in bytes.
int sanitizeBackgroundLabel(char (&label)[BackgroundLabelBytes]);
// The icon when it can be drawn, otherwise none (the face's generic icon).
inline const IconBitmap* usableIcon(const IconBitmap* icon) {
    return icon && icon->pixels && icon->width>0 && icon->height>0 ? icon : nullptr;
}
// Same look: icon reference and suggested colour, set or not and which.
inline bool sameStyle(const BackgroundInfo& a,const BackgroundInfo& b) {
    return a.icon==b.icon && a.suggestedColor==b.suggestedColor;
}
}
