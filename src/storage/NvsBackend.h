#pragma once
#include "storage/SettingsStore.h"
#include "storage/WatchPreferences.h"
namespace launcher {
// The default `nvs` partition, namespace `launcher`: the settings blob, and the
// watch face records beside it under their own keys (storage/WatchPreferences.h).
// The namespace is shared with whatever else MultiFirm hosts, so nothing here
// erases the partition: an unusable NVS means RAM defaults, not a wipe
// (plan.md 8.3).
class NvsBackend final : public SettingsBackend, public RecordBackend {
public:
    bool begin();
    bool ready() const { return ready_; }
    bool load(void* data, size_t& size) override;
    bool save(const void* data, size_t size) override;
    PrefResult read(const char* key, uint8_t* data, size_t& size) override;
    PrefResult write(const char* key, const uint8_t* data, size_t size) override;
private:
    bool ready_ = false;
};
}
