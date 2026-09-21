#pragma once
#include "storage/Settings.h"
namespace launcher {
// Explicit little-endian layout rather than a struct: the schema is first so an
// unknown version is recognised without having to trust the rest, and padding
// never becomes part of the stored format.
// [0..1] schema  [2] brightness  [3..4] screenOffSec
constexpr size_t SettingsRecordSize = 5;
class SettingsBackend {
public:
    virtual ~SettingsBackend() = default;
    // `size` is in/out: the capacity on the way in, the length read on the way
    // out. A missing record is a failure, not an empty one.
    virtual bool load(void* data, size_t& size) = 0;
    virtual bool save(const void* data, size_t size) = 0;
};
class SettingsStore {
public:
    static constexpr uint16_t Schema = 1;
    // Always leaves a usable value in RAM. Returns false when the stored record
    // could not be used, which is diagnosed but never repaired by erasing:
    // the namespace is shared with other firmware (plan.md 8.3).
    bool begin(SettingsBackend& backend);
    const Settings& get() const { return settings_; }
    // Writes once, on confirmation only. RAM is updated only if the write
    // lands, so what the launcher shows and what survives a reboot agree.
    bool save(const Settings& next);
    static size_t encode(const Settings& s, uint8_t* out);
    static bool decode(const uint8_t* in, size_t size, Settings& out, uint16_t& schema);
private:
    SettingsBackend* backend_ = nullptr;
    Settings settings_{};
};
}
