#pragma once
#include "core/Time.h"
#include <algorithm>
namespace launcher {
struct BatteryCurvePoint { int mv, percent; };
// Remaining charge by loaded battery voltage while discharging, measured on this
// device instead of M5Unified's straight line from 3.3 to 4.15 V. Share of the
// time left until the PMIC cut off, from two full drains at a steady load
// (M5StopWatch-MuteHid logs/drain-20260911, -20260916). Six partial drains at
// 9-66 mA (the same logs and docs/task8) fit the curve within 2 points, so
// the load's own voltage drop is left out.
inline constexpr BatteryCurvePoint BatteryCurve[] = {
    {4200,100}, {4150,98}, {4100,93}, {4050,89}, {4000,84}, {3950,79}, {3900,74},
    {3850,70}, {3800,64}, {3750,58}, {3700,52}, {3650,41}, {3600,29}, {3550,20},
    {3500,14}, {3450,8}, {3400,6}, {3350,5}, {3300,3}, {3200,1}, {3100,0},
};
// Linear between points; the ends clamp.
inline int batteryPercentFromMv(int mv) {
    constexpr int count = int(sizeof(BatteryCurve) / sizeof(BatteryCurve[0]));
    if (mv >= BatteryCurve[0].mv) return BatteryCurve[0].percent;
    for (int i = 1; i < count; ++i) {
        const auto& hi = BatteryCurve[i - 1];
        const auto& lo = BatteryCurve[i];
        if (mv < lo.mv) continue;
        const int span = hi.mv - lo.mv;
        return lo.percent + ((hi.percent - lo.percent) * (mv - lo.mv) + span / 2) / span;
    }
    return BatteryCurve[count - 1].percent;
}

// What the PMIC says at a battery reading. `changedAt` is when charging last
// started or stopped, or -1 if it has not since boot.
struct ChargeInput { bool powered = false, chargingKnown = false, charging = false; TimeUs changedAt = -1; };
// Turns a reading into the shown percent, on the discharge curve throughout so
// that plugging in or unplugging does not change the scale: while charging,
// the voltage less the charger's lift reads as the rested battery would.
class BatteryEstimator {
public:
    // The charger's constant current lifts the voltage over the rested battery:
    // 44 mV on average over the 16 rests of the 2026-10-02 charge (32-52 mV),
    // which leaves the charging value within 2 points of the rested one.
    static constexpr int ChargeLiftMv = 44;
    // Just after charging starts or stops the voltage is still moving: the
    // rests above needed about a minute to settle. Until then the value read
    // just before the change stays, and the voltage is not read at all.
    static constexpr TimeUs SettleUs = 60 * 1000000LL;
    // Only a value read within this before the change is held. An older one,
    // read before the panel went dark, may be hours out of date: charging in
    // the dark and unplugging just before a glance must not bring it back.
    static constexpr TimeUs HoldableUs = 60 * 1000000LL;
    // A charger idle on USB below this has paused or failed; it is not full.
    static constexpr int FullMinMv = 4100;
    // `readMv` returns the battery voltage; <= 0 is a failed read, unknown.
    template <class ReadMv>
    int update(TimeUs now, const ChargeInput& in, ReadMv readMv) {
        const bool settling = in.changedAt >= 0 && now - in.changedAt < SettleUs;
        if (settling && shown_ >= 0 && readAt_ <= in.changedAt && in.changedAt - readAt_ <= HoldableUs)
            return shown_;
        const int mv = readMv();
        if (mv <= 0) return -1;
        readAt_ = now;
        if (in.powered && in.chargingKnown && !in.charging && mv >= FullMinMv) return shown_ = 100;
        if (in.powered && in.chargingKnown && in.charging)
            return shown_ = std::min(batteryPercentFromMv(mv - ChargeLiftMv), 99); // 100 is the charger's word.
        return shown_ = batteryPercentFromMv(mv);
    }
private:
    int shown_ = -1;
    TimeUs readAt_ = 0;
};
}
