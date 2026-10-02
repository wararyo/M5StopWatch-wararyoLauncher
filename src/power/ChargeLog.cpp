#include "ChargeLog.h"
#ifdef LAUNCHER_CHARGE_LOG
#include <M5Unified.h>
#include <cstdio>
namespace launcher {
namespace {
// Charge for 10 minutes, then rest for 3. The rest is sampled every 10 seconds
// so the record also shows how long the voltage takes to settle.
constexpr TimeUs ChargeUs = 10 * 60 * 1000000LL;
constexpr TimeUs RestUs = 3 * 60 * 1000000LL;
constexpr TimeUs ChargeSampleUs = 60 * 1000000LL;
constexpr TimeUs RestSampleUs = 10 * 1000000LL;
constexpr TimeUs RetryUs = 1000000;
// USB power back within this continues the record: a PC restart drops VBUS
// for a minute or two. A longer absence is a new charge and a new record.
constexpr TimeUs ResumeUs = 5 * 60 * 1000000LL;
// A full charge from empty takes a few hours: about 200 charging samples and
// 20 rests of 18. On overflow the tail goes, never the start.
constexpr int Capacity = 2048;

// State bits, in the order tools/battery_charge.py names them.
enum : uint8_t { Paused = 1, Charging = 2, Lit = 4, Unplugged = 8 };
enum class Phase : uint8_t { Charge, Rest, Full };
const char* name(Phase p) { return p == Phase::Charge ? "charge" : p == Phase::Rest ? "rest" : "full"; }

struct Entry { uint32_t seconds; int16_t vbat; uint8_t state; };
Entry entries[Capacity];
int count = 0;
bool active = false, overflow = false, enablePending = false;
Phase phase = Phase::Charge;
TimeUs startedAt = 0, phaseStartedAt = 0, phaseEnd = 0, nextSampleAt = 0, retryAt = 0, unpluggedAt = 0;
int notCharging = 0;

// PWR_CFG bit 0 is the charge enable. Read-modify-write: the same register
// switches the rails, the 5V boost and the status LED, and a failed read
// writes nothing.
bool setCharge(bool on) {
    const bool ok = on ? M5.In_I2C.bitOn(0x6e, 0x06, 0x01, 100000) : M5.In_I2C.bitOff(0x6e, 0x06, 0x01, 100000);
    std::printf("[Charge] charge=%s%s\n", on ? "on" : "off", ok ? "" : " FAILED");
    return ok;
}
void enableCharge(TimeUs now) {
    enablePending = !setCharge(true);
    retryAt = now + RetryUs;
}
void record(TimeUs now, const PowerManager& power, uint8_t flags) {
    if (count >= Capacity) { overflow = true; return; }
    uint8_t state = flags;
    // The charger's own status pin, as of the last 1-second USB sample.
    if (power.usb.chargeValid && power.usb.charging) state |= Charging;
    if (!power.screenOff()) state |= Lit;
    entries[count++] = {uint32_t((now - startedAt) / 1000000), int16_t(M5.Power.getBatteryVoltage()), state};
}
void charge(TimeUs now) {
    enableCharge(now);
    phase = Phase::Charge;
    phaseStartedAt = now;
    phaseEnd = now + ChargeUs;
    nextSampleAt = now + ChargeSampleUs;
    notCharging = 0;
}
void rest(TimeUs now, const PowerManager& power) {
    record(now, power, 0); // The charging voltage right before the pause.
    if (!setCharge(false)) { phaseEnd = now + ChargeUs; return; } // Try the next cycle.
    phase = Phase::Rest;
    phaseEnd = now + RestUs;
    nextSampleAt = now + RestSampleUs;
}
void start(TimeUs now, const PowerManager& power) {
    count = 0;
    overflow = false;
    active = true;
    startedAt = now;
    charge(now);
    record(now, power, 0);
}
}
void beginChargeLog() {
    std::printf("[Charge] enabled: on USB power charges %llds then rests %llds; send C to dump\n",
                (long long)(ChargeUs / 1000000), (long long)(RestUs / 1000000));
    enableCharge(0);
}
void chargeLog(TimeUs now, const PowerManager& power) {
    if (enablePending && now >= retryAt) enableCharge(now);
    // An unanswered VBUS read decides nothing, as in the drain record.
    if (!power.usb.vbusValid) return;
    if (!active) {
        if (!power.usb.powered()) return;
        if (count > 0 && now - unpluggedAt < ResumeUs) {
            active = true;
            charge(now);
            record(now, power, 0);
            std::printf("[Charge] resumed after %llds without USB power\n", (long long)((now - unpluggedAt) / 1000000));
        } else {
            start(now, power);
        }
        return;
    }
    if (!power.usb.powered()) {
        // Unplugged: keep the record for the dump, and never leave charging off.
        if (phase == Phase::Rest) enableCharge(now);
        record(now, power, Unplugged); // Marks the gap in the record.
        phase = Phase::Charge;
        active = false;
        unpluggedAt = now;
        return;
    }
    switch (phase) {
    case Phase::Full:
        return;
    case Phase::Rest:
        if (now >= phaseEnd) {
            record(now, power, Paused); // The rested voltage.
            charge(now);
        } else if (now >= nextSampleAt) {
            record(now, power, Paused);
            nextSampleAt += RestSampleUs;
        }
        return;
    case Phase::Charge:
        if (now >= phaseEnd) { rest(now, power); return; }
        if (now < nextSampleAt) return;
        nextSampleAt += ChargeSampleUs;
        record(now, power, 0);
        // The status pin needs a moment after charging resumes; two idle
        // samples a minute apart mean the charger has finished.
        const bool idle = power.usb.chargeValid && !power.usb.charging && now - phaseStartedAt >= ChargeSampleUs;
        notCharging = idle ? notCharging + 1 : 0;
        if (notCharging >= 2) {
            phase = Phase::Full;
            std::printf("[Charge] full at t=%llds\n", (long long)((now - startedAt) / 1000000));
        }
        return;
    }
}
void dumpChargeLog() {
    std::printf("CHARGE source=ram charge_s=%lld rest_s=%lld active=%d phase=%s overflow=%d\n",
                (long long)(ChargeUs / 1000000), (long long)(RestUs / 1000000), int(active), name(phase),
                int(overflow));
    for (int i = 0; i < count; ++i)
        std::printf("CHARGE t=%lu vbat=%d st=%u\n", (unsigned long)entries[i].seconds, entries[i].vbat,
                    entries[i].state);
    std::printf("CHARGE end n=%d\n", count);
    std::fflush(stdout);
}
}
#endif
