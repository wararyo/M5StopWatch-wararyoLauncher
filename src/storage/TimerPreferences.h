#pragma once
#include "storage/WatchPreferences.h"
#include <cstdint>
namespace launcher {
// The length the timer was last started with, the next visit's starting value
// (docs/task12/plan.md 1.1, 2.5). Its own record beside the settings, so the
// settings schema does not change.
//
//   [0] format 1  [1..4] seconds, little endian, normalized (1..99:59:59)
class TimerPreferences {
public:
    static constexpr const char* Key="timer";
    static constexpr uint8_t Format=1;
    static constexpr size_t RecordBytes=5;
    static constexpr int32_t DefaultSeconds=180;
    void bind(RecordBackend* backend) { backend_=backend; }
    // Reads the record. Whatever it says, a usable value is left in value():
    // a missing, unknown or broken record means the default, and is not
    // rewritten until a timer is started.
    PrefResult load();
    int32_t value() const { return value_; }
    // The timer was started for `seconds`. It becomes the next starting value
    // at once, and is written only when it differs from what storage was last
    // known to hold, so starting the same length again costs no write. Ok
    // when nothing needed writing; a failure keeps the value in RAM, and the
    // next start compares with the last value that did land.
    PrefResult remember(int32_t seconds);
    static size_t encode(int32_t seconds,uint8_t* out);
    static bool decode(const uint8_t* in,size_t size,int32_t& seconds);
private:
    RecordBackend* backend_=nullptr;
    int32_t value_=DefaultSeconds;
    int32_t saved_=-1; // -1: storage holds nothing usable, as far as is known.
};
}
