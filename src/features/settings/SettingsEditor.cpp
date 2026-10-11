#include "SettingsEditor.h"
#include "i18n/Strings.h"
#include "storage/Settings.h"
namespace launcher {
namespace {
int wrap(int value,int low,int high,int delta,int step=1) {
    const int next=value+delta*step;
    if (next>high) return low;
    if (next<low) return high;
    return next;
}
}
void SettingsEditor::step(int delta) {
    int& value=model_.fields[model_.cursor];
    switch (model_.view) {
    case SettingsView::DateTime:
        switch (model_.cursor) {
        // The day is not folded into the month here: an impossible date is
        // rejected on save, so passing through 31 while picking a month works.
        case 0: value=wrap(value,TimeService::MinYear,TimeService::MaxYear,delta); break;
        case 1: value=wrap(value,1,12,delta); break;
        case 2: value=wrap(value,1,31,delta); break;
        case 3: value=wrap(value,0,23,delta); break;
        default: value=wrap(value,0,59,delta); break;
        }
        break;
    // Wrapping, not clamping: B only ever steps forward, so every level has to
    // be reachable without a second button.
    case SettingsView::Brightness:
        value=wrap(value,BrightnessMin,BrightnessMax,delta,BrightnessStep); break;
    case SettingsView::ScreenOff:
        value=wrap(value,0,ScreenOffChoiceCount-1,delta); break;
    default: break;
    }
}
void dateTimeFields(const TimeService& time,int (&fields)[5]) {
    std::tm jst{}; TimeUs subsecond=0;
    CivilTime start{TimeService::MinYear,1,1,0,0,0};
    if (time.now(jst,subsecond))
        start={jst.tm_year+1900,jst.tm_mon+1,jst.tm_mday,jst.tm_hour,jst.tm_min,0};
    fields[0]=start.year; fields[1]=start.month; fields[2]=start.day;
    fields[3]=start.hour; fields[4]=start.minute;
}
SaveResult saveDateTime(TimeService& time,const int (&fields)[5]) {
    return time.save({fields[0],fields[1],fields[2],fields[3],fields[4],0});
}
const char* dateTimeNotice(SaveResult result) {
    switch (result) {
    case SaveResult::Saved: return text::TimeSaved;
    case SaveResult::Invalid: return text::InvalidDate;
    case SaveResult::RtcWriteFailed: return text::SaveFailed;
    default: return text::ClockUnavailable;
    }
}
}
