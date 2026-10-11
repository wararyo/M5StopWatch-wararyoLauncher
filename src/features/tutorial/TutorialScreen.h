#pragma once
#include "host/Screen.h"
#include "features/settings/SettingsEditor.h"
#include "features/tutorial/TutorialModel.h"
#include "storage/TutorialRecord.h"
namespace launcher {
// The first-boot tutorial (docs/task15/plan.md): the date, if the clock is
// unset, then how to go home, which is finished by going home from there.
// The host only opens it when the record says it is due; everything after
// that is the tutorial's own. It is in no list, and nothing else opens it.
class TutorialScreen final : public Screen {
public:
    void resize(int width,int height) override { editor_.resize(width,height); }
    // Without a clock the date step is skipped. Without the record the
    // tutorial cannot be finished, so it does not open.
    void bindTime(TimeService* time) { time_=time; }
    void bindRecord(TutorialRecord* record) { record_=record; }
    bool available() const override { return record_!=nullptr; }
    // The date first while the clock is unset; otherwise straight to the way home.
    void enter(TimeUs now) override;
    // Whatever step it was in, it starts over from the clock next time. Set
    // after wentHome(), which reads the step.
    void exit() override { step_=TutorialStep::DateTime; editor_.hold()=HoldRepeat{}; }
    ScreenOutcome handle(const Events& e,TimeUs now) override;
    TimeUs nextUpdate() const override { return step_==TutorialStep::DateTime ? editor_.nextUpdate() : INT64_MAX; }
    bool tick(TimeUs now) override { return step_==TutorialStep::DateTime && editor_.tick(now); }
    // Home from the way home finishes the tutorial. Home from the date step
    // does not: it may have been a stray touch (docs/task15/plan.md 1.3).
    void wentHome() override;
    TutorialModel model() const { return {step_,editor_.model()}; }
private:
    // The date step's buttons: save and go on, or go on with the clock unset.
    const char* confirm();
    TimeService* time_=nullptr;
    TutorialRecord* record_=nullptr;
    TutorialStep step_=TutorialStep::DateTime;
    SettingsEditor editor_;
};
}
