#include "MultiFirmAdapter.h"
#include <multifirm_host.h>
#include <esp_err.h>
#include <freertos/task.h>
#include <cstdio>
namespace launcher {
namespace {
SlotStatus translate(multifirm::host::SlotState state) {
    switch (state) {
    case multifirm::host::SlotState::Ready: return SlotStatus::Ready;
    case multifirm::host::SlotState::Empty: return SlotStatus::Empty;
    case multifirm::host::SlotState::Invalid: return SlotStatus::Invalid;
    case multifirm::host::SlotState::ReadError: break;
    }
    return SlotStatus::ReadError;
}
}
void MultiFirmAdapter::begin() {
    lock_=xSemaphoreCreateMutex();
    // One flash read of the whole partition table; never called again.
    catalog_.layoutSupported=multifirm::host::isMultiFirmLayout();
    for (auto& entry:catalog_.slots)
        entry.status=catalog_.layoutSupported ? SlotStatus::Scanning : SlotStatus::Unsupported;
    std::printf("[Slots] layout=%s\n",catalog_.layoutSupported ? "supported" : "unsupported");
    publish();
}
void MultiFirmAdapter::publish() {
    generation_.fetch_add(1,std::memory_order_release);
}
bool MultiFirmAdapter::poll(SlotCatalog& out) {
    const auto generation=generation_.load(std::memory_order_acquire);
    if (generation==seen_) return false;
    if (lock_ && xSemaphoreTake(lock_,0)!=pdTRUE) return false; // Retry next pass.
    out=catalog_;
    seen_=generation;
    if (lock_) xSemaphoreGive(lock_);
    return true;
}
void MultiFirmAdapter::requestScan() {
    if (started_ || !catalog_.layoutSupported) return;
    started_=true;
    // CPU0: the UI task is pinned to CPU1 (CONFIG_ESP_MAIN_TASK_AFFINITY_CPU1).
    // 8 KiB matches the main task, which is what MultiFirm sized its 512 byte
    // read blocks against.
    xTaskCreatePinnedToCore(scanTask,"mf-scan",8192,this,1,nullptr,0);
}
void MultiFirmAdapter::scanTask(void* context) {
    static_cast<MultiFirmAdapter*>(context)->scan();
    vTaskDelete(nullptr);
}
void MultiFirmAdapter::scan() {
    for (int slot=1;slot<=SlotCount;++slot) {
        multifirm::host::SlotInfo info;
        // A non-OK return is a layout or argument problem, never a bad slot:
        // per-slot failures arrive as ESP_OK with a state of ReadError.
        const auto error=multifirm::host::inspectSlot(slot,info);
        if (lock_) xSemaphoreTake(lock_,portMAX_DELAY);
        if (error!=ESP_OK) {
            catalog_.layoutSupported=false;
            for (auto& entry:catalog_.slots) { entry=SlotEntry{}; entry.status=SlotStatus::Unsupported; }
            if (lock_) xSemaphoreGive(lock_);
            std::printf("[Slots] scan aborted: %s\n",esp_err_to_name(error));
            publish();
            return;
        }
        auto& entry=catalog_.slots[slot-1];
        entry=SlotEntry{};
        entry.status=translate(info.state);
        entry.error=int32_t(info.error);
        if (entry.status==SlotStatus::Ready) {
            std::snprintf(entry.name,sizeof(entry.name),"%s",info.name.c_str());
            std::snprintf(entry.version,sizeof(entry.version),"%s",info.version.c_str());
        }
        if (lock_) xSemaphoreGive(lock_);
        std::printf("[Slots] slot=%d state=%s name=%s version=%s error=%s\n",slot,
                    multifirm::host::slotStateName(info.state),entry.name,entry.version,
                    esp_err_to_name(info.error));
        publish();
        // Hand CPU0 back to its idle task between images.
        vTaskDelay(1);
    }
}
void MultiFirmAdapter::shutdown(void*) {
    // Runs on the UI task, after the boot partition has been set and before the
    // restart. Task 5 stops the stopwatch here; there is nothing else to stop.
    std::printf("[Slots] boot committed; shutting down\n");
}
bool MultiFirmAdapter::boot(int slot,const char** message) {
    std::printf("[Slots] booting slot=%d\n",slot);
    // Returns only when it failed: a success ends in esp_restart().
    const auto error=multifirm::host::bootSlot(slot,shutdown,this);
    std::snprintf(message_,sizeof(message_),"%s",esp_err_to_name(error));
    std::printf("[Slots] boot failed: %s\n",message_);
    if (message) *message=message_;
    return false;
}
}
