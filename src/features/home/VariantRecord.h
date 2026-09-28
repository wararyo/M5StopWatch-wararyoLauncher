#pragma once
#include "features/home/faces/TimeGroups.h"
#include "storage/WatchPreferences.h"
namespace launcher {
// A face's time variant across restarts, in the face's own record: version 1,
// then 0 (hours and minutes) or 1 (with seconds). Digital and Forest both keep
// theirs this way; nothing makes a face keep anything (docs/task10/plan-10-5.md 4).
//
// Read once, when the face is registered. A missing, unknown or broken record
// gives the fallback and is left as it is: only a variant the user chose is
// ever written. What is known to be stored is kept apart from what is shown,
// so the same value is not written twice, and a write that failed is tried
// again on the next change even if the value comes back to the old one.
class VariantRecord {
public:
    static constexpr uint8_t Version=1;
    void bind(FacePreferences* prefs) { prefs_=prefs; known_=false; }
    TimeVariant restore(TimeVariant fallback) {
        if (!prefs_) return fallback;
        uint8_t payload[FacePayloadMax];
        size_t size=0;
        if (prefs_->load(payload,sizeof(payload),size)!=PrefResult::Ok || size!=2 ||
            payload[0]!=Version || payload[1]>1) return fallback;
        stored_=payload[1] ? TimeVariant::HourMinuteSecond : TimeVariant::HourMinute;
        known_=true;
        return stored_;
    }
    // True when the variant is stored (or nothing is bound to store it in).
    bool save(TimeVariant variant) {
        if (!prefs_ || (known_ && stored_==variant)) return true;
        const uint8_t payload[2]={Version,uint8_t(variant==TimeVariant::HourMinuteSecond)};
        if (prefs_->save(payload,sizeof(payload))!=PrefResult::Ok) return false;
        stored_=variant; known_=true;
        return true;
    }
private:
    FacePreferences* prefs_=nullptr;
    bool known_=false;
    TimeVariant stored_=TimeVariant::HourMinute;
};
}
