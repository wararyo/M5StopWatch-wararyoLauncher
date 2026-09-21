#pragma once
#include "storage/SettingsStore.h"
namespace launcher {
// The default `nvs` partition, namespace `launcher`, one blob. The namespace is
// shared with whatever else MultiFirm hosts, so nothing here erases the
// partition: an unusable NVS means RAM defaults, not a wipe (plan.md 8.3).
class NvsBackend final : public SettingsBackend {
public:
    bool begin();
    bool ready() const { return ready_; }
    bool load(void* data, size_t& size) override;
    bool save(const void* data, size_t size) override;
private:
    bool ready_ = false;
};
}
