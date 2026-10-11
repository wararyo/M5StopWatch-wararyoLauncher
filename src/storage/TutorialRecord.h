#pragma once
#include "storage/WatchPreferences.h"
#include <cstdint>
namespace launcher {
// Whether the first-boot tutorial has been finished (docs/task15/plan.md 2.4).
// Its own record beside the settings, so the settings schema does not change.
// It says only that the tutorial was finished: there is no other value.
//
//   [0] format 1
class TutorialRecord {
public:
    static constexpr const char* Key="tutorial";
    static constexpr uint8_t Format=1;
    static constexpr size_t RecordBytes=1;
    void bind(RecordBackend* backend) { backend_=backend; }
    bool bound() const { return backend_!=nullptr; }
    // Reads the record. The tutorial is due only when there is none at all:
    // a record of a format this build does not know, or storage that cannot
    // be read, never brings it up, so a broken record cannot show it on every
    // boot. Unbound (the render check), it is never due.
    PrefResult load();
    bool due() const { return due_; }
    // The tutorial was finished: the record is written. A failure leaves it
    // due, and the next boot shows the tutorial again.
    PrefResult complete();
private:
    RecordBackend* backend_=nullptr;
    bool due_=false;
};
}
