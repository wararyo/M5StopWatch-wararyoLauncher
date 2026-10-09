#pragma once
#include "host/Screen.h"
#include "input/HoldRepeat.h"
#include "services/TimeService.h"
#include "features/settings/SettingsLayout.h"
namespace launcher {
// The editor and information views' input (docs/task12/plan.md 1.5), apart
// from what they edit: A moves the focus over the fields, actions and
// buttons; B steps a field, repeating while held, and carries out an action
// or a button. There is no separate editing state to enter. What a button
// saves and where the view goes then belong to the owner (settings, and the
// tutorial's date step, docs/task15/plan.md 2.2), which is asked through
// `confirm` with the focus already on what was pressed.
class SettingsEditor {
public:
    void resize(int width,int height) { viewport_={width,height}; }
    // A new view, focused on its first slot. Its fields are the owner's to
    // fill. A press held across the change belongs to the view it began in:
    // its release must not decide anything in this one.
    void open(SettingsView view) { model_.view=view; model_.cursor=0; hold_.follow(-1); }
    const SettingsEditorModel& model() const { return model_; }
    SettingsEditorModel& model() { return model_; }
    // The press of B being followed, which outlives the view it began in.
    HoldRepeat& hold() { return hold_; }
    TimeUs nextUpdate() const { return hold_.nextUpdate(); }
    // A step of the held field, if one is due.
    bool tick(TimeUs now) {
        if (!hold_.take(now)) return false;
        step(1);
        return true;
    }
    // Steps the focused field, wrapping within its range.
    void step(int delta);
    // `confirm(ScreenOutcome&)` carries out the focused action or button and
    // returns the notice to show, if any. It may open another view.
    template<class Confirm>
    ScreenOutcome handle(const Events& e,Confirm&& confirm);
private:
    Viewport viewport_{468,468};
    SettingsEditorModel model_{};
    // B held on a field steps it (docs/task12/plan.md 1.5).
    HoldRepeat hold_;
};
// The date editor's fields (year, month, day, hour, minute): the time now, or
// the first moment the clock trusts when it is unset.
void dateTimeFields(const TimeService& time,int (&fields)[5]);
// The fields as JST, set on the clock; the notice that tells how it went.
SaveResult saveDateTime(TimeService& time,const int (&fields)[5]);
const char* dateTimeNotice(SaveResult result);

template<class Confirm>
ScreenOutcome SettingsEditor::handle(const Events& e,Confirm&& confirm) {
    ScreenOutcome out{};
    const int fields=settingsFieldCount(model_.view);
    // B on a field steps it as it goes down (input/HoldRepeat.h).
    if (e.holdChanged && e.hold==Hold::B && hold_.begin(e.holdSince,model_.cursor,model_.cursor<fields)) {
        step(1); out.changed=true;
    }
    if (e.gesture==Gesture::Tap) {
        const auto hit=hitSettings({viewport_,model_.view,model_.cursor},e.x,e.y);
        switch (hit.kind) {
        case SettingsHit::Field:
            model_.cursor=hit.index; out.changed=true; break;
        case SettingsHit::Up:
        case SettingsHit::Down:
            model_.cursor=hit.index;
            step(hit.kind==SettingsHit::Up ? 1 : -1); out.changed=true; break;
        case SettingsHit::Action:
            model_.cursor=fields+hit.index;
            out.changed=true; out.notice=confirm(out); break;
        case SettingsHit::Button:
            model_.cursor=fields+settingsActionCount(model_.view)+hit.index;
            out.changed=true; out.notice=confirm(out); break;
        case SettingsHit::None: break;
        }
        hold_.follow(model_.cursor);
        // B let go in this same sample: the tap decided it, but the hold ends
        // here all the same, or its repeats would run on with nothing held.
        if (e.decide || (e.holdChanged && e.hold!=Hold::B)) hold_.end();
        return out;
    }
    if (e.next) {
        model_.cursor=(model_.cursor+1)%settingsSlotCount(model_.view);
        hold_.follow(model_.cursor);
        out.changed=true;
    }
    if (e.decide) {
        if (hold_.release(model_.cursor)) {
            if (model_.cursor<fields) step(1);
            else out.notice=confirm(out);
        }
        out.changed=true;
    } else if (e.holdChanged && e.hold!=Hold::B) hold_.end();
    return out;
}
}
