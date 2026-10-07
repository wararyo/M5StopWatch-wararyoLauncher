#pragma once
#include "storage/WatchPreferences.h"
#include <cstdint>
namespace launcher {
// Today's steps as last saved, so a restart on the same day carries them on
// (docs/task13/plan.md 2.5). Its own record beside the settings, so the
// settings schema does not change. Only one day is kept.
//
//   [0] format 1  [1..4] day (int32)  [5..8] steps (uint32), little endian
class PedometerRecord {
public:
    static constexpr const char* Key="pedometer";
    static constexpr uint8_t Format=1;
    static constexpr size_t RecordBytes=9;
    void bind(RecordBackend* backend) { backend_=backend; }
    // Reads the record. Ok leaves the day and its steps in day() and steps();
    // anything else leaves none (has() false), and the record is not touched
    // until the next save overwrites it.
    PrefResult load();
    bool has() const { return has_; }
    int32_t day() const { return day_; }
    uint32_t steps() const { return steps_; }
    // Writes `steps` for `day` when that differs from what storage was last
    // known to hold, so a save with nothing new costs no write. Ok when
    // nothing needed writing. A failure is not retried here: the next save
    // compares with the last value that did land.
    PrefResult remember(int32_t day,uint32_t steps);
    static size_t encode(int32_t day,uint32_t steps,uint8_t* out);
    static bool decode(const uint8_t* in,size_t size,int32_t& day,uint32_t& steps);
private:
    RecordBackend* backend_=nullptr;
    bool has_=false;
    int32_t day_=0;
    uint32_t steps_=0;
};
}
