#include "DrainLog.h"
#ifdef LAUNCHER_DRAIN_LOG
#include <M5Unified.h>
#include <nvs.h>
#include <esp_pm.h>
#include <freertos/FreeRTOS.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
namespace launcher {
namespace {
// Same shape as MuteHid's PowerProbe record: fine in RAM, coarse in NVS. RAM
// is lost on reset but a flat battery ends a run with one, so the NVS copy is
// what tells when the device stopped.
constexpr TimeUs RecordIntervalUs = 60 * 1000000LL;
constexpr int Capacity = 24 * 60;
constexpr TimeUs PersistIntervalUs = 15 * 60 * 1000000LL;
constexpr int PersistCapacity = 32 * 4;
// Its own namespace, so the settings schema in `launcher` never sees it.
constexpr const char* Namespace = "launcher_drn";
constexpr const char* PersistKey = "drain";
constexpr uint16_t PersistVersion = 1;

// State bits, in the order tools/battery_drain.py names them.
enum : uint8_t { Lit = 1, Charging = 2, Usb = 4 };

struct Entry { uint32_t seconds; int16_t vbat; uint8_t state; };
Entry entries[Capacity];
int head = 0, count = 0; // head is the next slot to write.
bool active = false;
TimeUs startedAt = 0, nextAt = 0, nextPersistAt = 0;

struct Persisted {
    uint16_t version, intervalS, count;
    int16_t vbat[PersistCapacity];
    uint8_t state[PersistCapacity];
};
Persisted persisted{};

// Time actually spent in automatic light sleep during the run (work 8-5), so
// "enabled" is never mistaken for "slept". Written from the idle task.
portMUX_TYPE sleepMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t sleepCount = 0;
int64_t sleptUs = 0;
esp_err_t onSleepExit(int64_t sleepTimeUs, void*) {
    portENTER_CRITICAL_SAFE(&sleepMux);
    ++sleepCount; sleptUs += sleepTimeUs;
    portEXIT_CRITICAL_SAFE(&sleepMux);
    return ESP_OK;
}
void resetSleep() {
    portENTER_CRITICAL(&sleepMux);
    sleepCount = 0; sleptUs = 0;
    portEXIT_CRITICAL(&sleepMux);
}
void printSleep() {
    portENTER_CRITICAL(&sleepMux);
    const uint32_t n = sleepCount;
    const int64_t us = sleptUs;
    portEXIT_CRITICAL(&sleepMux);
    std::printf("DRAIN sleep count=%lu slept_s=%lld\n", (unsigned long)n, (long long)(us / 1000000));
}

void save() {
    nvs_handle_t nvs;
    auto err = nvs_open(Namespace, NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        err = nvs_set_blob(nvs, PersistKey, &persisted, sizeof(persisted));
        if (err == ESP_OK) err = nvs_commit(nvs);
        nvs_close(nvs);
    }
    // No print here: on battery nobody is listening. The dump reports the count.
}
bool load(Persisted& out) {
    nvs_handle_t nvs;
    if (nvs_open(Namespace, NVS_READONLY, &nvs) != ESP_OK) return false;
    size_t size = sizeof(out);
    const auto err = nvs_get_blob(nvs, PersistKey, &out, &size);
    nvs_close(nvs);
    return err == ESP_OK && size == sizeof(out) && out.version == PersistVersion &&
        out.count <= PersistCapacity;
}
void record(TimeUs now, const PowerManager& power, bool ram, bool nvs) {
    const int16_t vbat = int16_t(M5.Power.getBatteryVoltage());
    uint8_t state = 0;
    if (!power.screenOff()) state |= Lit;
    if (M5.Power.isCharging() == m5::Power_Class::is_charging) state |= Charging;
    if (power.usb.powered()) state |= Usb;
    if (ram) {
        entries[head] = {uint32_t((now - startedAt) / 1000000), vbat, state};
        head = (head + 1) % Capacity;
        if (count < Capacity) ++count;
    }
    if (nvs && persisted.count < PersistCapacity) { // Keep the first 32 hours.
        persisted.vbat[persisted.count] = vbat;
        persisted.state[persisted.count] = state;
        ++persisted.count;
        save();
    }
}
void start(TimeUs now, const PowerManager& power) {
    head = count = 0;
    startedAt = now;
    active = true;
    resetSleep();
    persisted = {PersistVersion, uint16_t(PersistIntervalUs / 1000000), 0, {}, {}};
    record(now, power, true, true);
    nextAt = now + RecordIntervalUs;
    nextPersistAt = now + PersistIntervalUs;
}
void dump() {
    if (count > 0) {
        std::printf("DRAIN source=ram interval=%llds active=%d\n",
                    (long long)(RecordIntervalUs / 1000000), int(active));
        const int first = (head - count + Capacity) % Capacity;
        for (int i = 0; i < count; ++i) {
            const auto& e = entries[(first + i) % Capacity];
            std::printf("DRAIN t=%lu vbat=%d st=%u\n", (unsigned long)e.seconds, e.vbat, e.state);
        }
        printSleep(); // RAM only, like the 1-minute record: lost on a restart.
        std::printf("DRAIN end n=%d nvs_n=%u\n", count, persisted.count);
    } else {
        // Nothing in RAM: the chip restarted, e.g. after the battery ran flat.
        Persisted saved{};
        const bool found = load(saved);
        const unsigned n = found ? saved.count : 0;
        std::printf("DRAIN source=nvs interval=%us active=%d\n", found ? saved.intervalS : 0, int(active));
        for (unsigned i = 0; i < n; ++i)
            std::printf("DRAIN t=%lu vbat=%d st=%u\n", (unsigned long)i * saved.intervalS,
                        saved.vbat[i], saved.state[i]);
        std::printf("DRAIN end n=%u\n", n);
    }
    std::fflush(stdout);
}
}
void beginDrainLog() {
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    esp_pm_sleep_cbs_register_config_t callbacks{};
    callbacks.exit_cb = onSleepExit;
    const auto err = esp_pm_light_sleep_register_cbs(&callbacks);
    std::printf("[Drain] enabled: records battery voltage while USB power is absent; send O to dump; "
                "sleep counter=%s\n", esp_err_to_name(err));
}
void drainLog(TimeUs now, const PowerManager& power) {
    char c;
    for (int i = 0; i < 16 && read(STDIN_FILENO, &c, 1) == 1; ++i)
        if (c == 'O') dump();
    // An unanswered VBUS read decides nothing: it must neither start a run
    // nor end one.
    if (!power.usb.vbusValid) return;
    if (!active) {
        if (!power.usb.powered()) start(now, power);
        return;
    }
    if (power.usb.powered()) {
        // Keep the record for the dump; the next unplug starts a new one.
        active = false;
        return;
    }
    const bool ram = now >= nextAt, nvs = now >= nextPersistAt;
    if (!ram && !nvs) return;
    record(now, power, ram, nvs);
    if (ram) nextAt += RecordIntervalUs;
    if (nvs) nextPersistAt += PersistIntervalUs;
}
}
#endif
