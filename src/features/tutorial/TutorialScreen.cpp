#include "TutorialScreen.h"
#include <cstdio>
namespace launcher {
void TutorialScreen::enter(TimeUs) {
    editor_.hold()=HoldRepeat{};
    if (time_ && !time_->valid()) {
        step_=TutorialStep::DateTime;
        editor_.open(SettingsView::DateTime);
        dateTimeFields(*time_,editor_.model().fields);
    } else {
        step_=TutorialStep::Guide;
    }
}
const char* TutorialScreen::confirm() {
    const auto& m=editor_.model();
    // Save, then cancel: the date editor has no actions.
    if (m.cursor==settingsFieldCount(m.view)+1) { step_=TutorialStep::Guide; return nullptr; }
    const SaveResult result=time_ ? saveDateTime(*time_,m.fields) : SaveResult::Unrecoverable;
    if (result==SaveResult::Saved) step_=TutorialStep::Guide;
    return dateTimeNotice(result);
}
ScreenOutcome TutorialScreen::handle(const Events& e,TimeUs) {
    // The way home has nothing to press: only home itself, which the host
    // takes before any screen.
    if (step_==TutorialStep::Guide) return {};
    return editor_.handle(e,[this](ScreenOutcome&) { return confirm(); });
}
void TutorialScreen::wentHome() {
    if (step_!=TutorialStep::Guide || !record_) return;
    const auto result=record_->complete();
    std::printf("[Tutorial] finished record=%s\n",prefResultName(result));
}
}
