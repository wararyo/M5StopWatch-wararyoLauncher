#pragma once
#include "SlotService.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>
namespace launcher {
// MultiFirm's host API behind the launcher's port.
//
// Scanning reads each guest image twice and hashes it (about 320 ms per slot on
// this device), so it runs on its own task pinned to CPU0 while the UI task
// keeps CPU1. Booting does not: `bootSlot` runs its shutdown callback on the
// calling task, and that callback has to stop things the UI task owns.
class MultiFirmAdapter final : public SlotService {
public:
    void begin();
    void requestScan() override;
    bool poll(SlotCatalog& out) override;
    bool boot(int slot,const char** message) override;
private:
    static void scanTask(void* context);
    static void shutdown(void* context);
    void scan();
    void publish();
    SemaphoreHandle_t lock_=nullptr;
    SlotCatalog catalog_{};
    std::atomic<uint32_t> generation_{0};
    uint32_t seen_=0;
    bool started_=false;
    char message_[48]{};
};
}
