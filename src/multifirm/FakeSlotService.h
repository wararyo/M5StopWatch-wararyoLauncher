#pragma once
#include "SlotService.h"
#include <cstdio>
namespace launcher {
// Injects slot states that cannot be produced safely on a device: a corrupt
// image, a read failure, a foreign partition table. Used by the host tests and
// by the render check build, never by the normal firmware.
class FakeSlotService final : public SlotService {
public:
    SlotCatalog catalog{};
    bool bootSucceeds=false;          // A real success never returns.
    const char* bootMessage="ESP_ERR_IMAGE_INVALID";
    int scanRequests=0,bootRequests=0,bootSlot=0;
    bool pending=false;               // Set by publish(), cleared by poll().
    void set(int slot,SlotStatus status,const char* name=nullptr,
             const char* version=nullptr,int32_t error=0) {
        auto& entry=catalog.slots[slot-1];
        entry=SlotEntry{}; entry.status=status; entry.error=error;
        if (name) std::snprintf(entry.name,sizeof(entry.name),"%s",name);
        if (version) std::snprintf(entry.version,sizeof(entry.version),"%s",version);
        pending=true;
    }
    void setUnsupported() {
        catalog.layoutSupported=false;
        for (auto& entry:catalog.slots) { entry=SlotEntry{}; entry.status=SlotStatus::Unsupported; }
        pending=true;
    }
    void requestScan() override { ++scanRequests; }
    bool poll(SlotCatalog& out) override {
        if (!pending) return false;
        pending=false; out=catalog; return true;
    }
    bool boot(int slot,const char** message) override {
        ++bootRequests; bootSlot=slot;
        if (bootSucceeds) return true;   // Only a test can take this branch.
        if (message) *message=bootMessage;
        return false;
    }
};
}
