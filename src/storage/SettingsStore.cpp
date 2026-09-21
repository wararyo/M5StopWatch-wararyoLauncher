#include "SettingsStore.h"
#include <cstdio>
namespace launcher {
size_t SettingsStore::encode(const Settings& s, uint8_t* out) {
    out[0] = uint8_t(Schema & 0xff); out[1] = uint8_t(Schema >> 8);
    out[2] = s.brightness;
    out[3] = uint8_t(s.screenOffSec & 0xff); out[4] = uint8_t(s.screenOffSec >> 8);
    return SettingsRecordSize;
}
bool SettingsStore::decode(const uint8_t* in, size_t size, Settings& out, uint16_t& schema) {
    if (size != SettingsRecordSize) return false;
    schema = uint16_t(in[0] | (in[1] << 8));
    out.brightness = in[2];
    out.screenOffSec = uint16_t(in[3] | (in[4] << 8));
    return true;
}
bool SettingsStore::begin(SettingsBackend& backend) {
    backend_ = &backend;
    settings_ = Settings{};
    uint8_t raw[SettingsRecordSize]{};
    size_t size = sizeof(raw);
    if (!backend.load(raw, size)) {
        std::printf("[Settings] no stored record; using defaults\n");
        return false;
    }
    Settings stored{};
    uint16_t schema = 0;
    if (!decode(raw, size, stored, schema)) {
        std::printf("[Settings] record size %u unusable; using defaults\n", unsigned(size));
        return false;
    }
    if (schema != Schema) {
        // A newer launcher may have written this. Leave it alone rather than
        // overwrite a format this build does not understand.
        std::printf("[Settings] schema %u != %u; using defaults without rewriting\n",
                    unsigned(schema), unsigned(Schema));
        return false;
    }
    // Per field, so one bad value does not discard the other.
    bool ok = true;
    if (validBrightness(stored.brightness)) settings_.brightness = stored.brightness;
    else { std::printf("[Settings] brightness %u out of range\n", unsigned(stored.brightness)); ok = false; }
    if (validScreenOff(stored.screenOffSec)) settings_.screenOffSec = stored.screenOffSec;
    else { std::printf("[Settings] screen-off %us not a choice\n", unsigned(stored.screenOffSec)); ok = false; }
    std::printf("[Settings] brightness=%u screen_off=%us stored=%s\n",
                unsigned(settings_.brightness), unsigned(settings_.screenOffSec), ok ? "ok" : "partial");
    return ok;
}
bool SettingsStore::save(const Settings& next) {
    if (!validSettings(next)) return false;
    if (!backend_) return false;
    uint8_t raw[SettingsRecordSize]{};
    if (!backend_->save(raw, encode(next, raw))) {
        std::printf("[Settings] write failed; keeping brightness=%u screen_off=%us\n",
                    unsigned(settings_.brightness), unsigned(settings_.screenOffSec));
        return false;
    }
    settings_ = next;
    return true;
}
}
