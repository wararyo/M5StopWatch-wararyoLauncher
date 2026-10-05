#pragma once
#include "input/InputController.h"
#include <cstdint>
namespace launcher {
// A screen's reading of one button held on its own as repeated steps
// (docs/task12/plan.md 2.3). Input only reports that the button is held and
// since when; this remembers what the press began on and times the repeats
// through the screen's own deadline, so no input-period sampling is needed.
//
// A press belongs to the target it began on. Once the focus leaves that
// target (a tap on another field) the press is spent: it stops repeating, and
// its release does nothing, rather than acting on whatever is focused then.
// Once it has repeated, the release adds nothing either: the steps already
// were the press.
class HoldRepeat {
public:
    static constexpr TimeUs DelayUs=500000, PeriodUs=100000;
    // A hold began on `target`, `since` the button went down. Only a target
    // that `repeats` steps while held; any other is followed only to know
    // whether its release still acts.
    void begin(TimeUs since,int target,bool repeats) {
        following_=true; spent_=repeated_=false; target_=target;
        next_=repeats ? since+DelayUs : INT64_MAX;
    }
    // The focus is now `target`. A press that began elsewhere is spent.
    void follow(int target) {
        if (following_ && target!=target_) { spent_=true; next_=INT64_MAX; }
    }
    // The hold ended without a release of its own (the other button joined,
    // or the screen dropped it). Nothing of it acts any more.
    void end() { following_=false; next_=INT64_MAX; }
    // The button came up on `target`: whether that release acts. A release
    // that was never followed acts, so a screen entered mid-press, or a test
    // that only sends the release, behaves as before.
    bool release(int target) {
        const bool acts=!following_ || (!spent_ && !repeated_ && target==target_);
        end();
        return acts;
    }
    // A step if one is due by `now`. Late steps are not replayed: a stalled
    // loop adds one step and times the next from here.
    bool take(TimeUs now) {
        if (now<next_) return false;
        repeated_=true;
        next_+=PeriodUs;
        if (next_<=now) next_=now+PeriodUs;
        return true;
    }
    TimeUs nextUpdate() const { return next_; }
    bool following() const { return following_; }
    int target() const { return target_; }
private:
    bool following_=false,spent_=false,repeated_=false;
    int target_=-1;
    TimeUs next_=INT64_MAX;
};
}
