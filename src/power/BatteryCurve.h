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
// Charge by voltage while the charger holds its constant current, which lifts
// the voltage about 44 mV over the rested battery. Share of the charging time,
// from one charge after a drain to cut-off (docs/battery-curve, 2026-10-02):
// the current held steady, and about 5 minutes of charging preceded the record.
// Provisional until a second charge from a known empty start confirms it.
inline constexpr BatteryCurvePoint ChargingCurve[] = {
    {4200,90}, {4150,86}, {4100,82}, {4050,78}, {4000,74}, {3950,69}, {3900,64},
    {3850,58}, {3800,53}, {3750,46}, {3700,33}, {3650,21}, {3600,16}, {3550,9},
    {3500,5}, {3450,4}, {3400,2}, {3300,0},
};
// Linear between points; the ends clamp.
template <int N>
int percentOnCurve(const BatteryCurvePoint (&curve)[N], int mv) {
    if (mv >= curve[0].mv) return curve[0].percent;
    for (int i = 1; i < N; ++i) {
        const auto& hi = curve[i - 1];
        const auto& lo = curve[i];
        if (mv < lo.mv) continue;
        const int span = hi.mv - lo.mv;
        return lo.percent + ((hi.percent - lo.percent) * (mv - lo.mv) + span / 2) / span;
    }
    return curve[N - 1].percent;
}
inline int batteryPercentFromMv(int mv) { return percentOnCurve(BatteryCurve, mv); }
inline int chargingPercentFromMv(int mv) { return percentOnCurve(ChargingCurve, mv); }

// What the PMIC says at a battery reading.
struct ChargeInput { bool powered = false, chargingKnown = false, charging = false; };
// Turns a reading into the shown percent: the discharge curve, the charging
// curve while charging, then time at 4.2 V. Each reading stands on its own
// apart from that time, so noise and the voltage jump at plugging or
// unplugging show as they are.
class BatteryEstimator {
public:
    // The charger reaches 4.2 V at about 90% (ChargingCurve), then tapers the
    // current; it took 15 minutes from there to finish.
    static constexpr int ConstantVoltageMv = 4200, ConstantVoltagePercent = 90;
    static constexpr TimeUs ConstantVoltageUs = 15 * 60 * 1000000LL;
    // A charger idle on USB below this has paused or failed; it is not full.
    static constexpr int FullMinMv = 4100;
    // mv <= 0 is a failed read: unknown.
    int update(TimeUs now, int mv, const ChargeInput& in) {
        if (mv <= 0) return -1;
        if (in.powered && in.chargingKnown && !in.charging && mv >= FullMinMv) {
            constantVoltageSince_ = -1;
            return 100;
        }
        // M5Unified reads a failed status read as charging; on battery, ignore it.
        if (in.powered && in.chargingKnown && in.charging) {
            if (mv >= ConstantVoltageMv && constantVoltageSince_ < 0) constantVoltageSince_ = now;
            int p = chargingPercentFromMv(mv);
            if (constantVoltageSince_ >= 0)
                p = std::max(p, ConstantVoltagePercent + int((now - constantVoltageSince_) *
                    (100 - ConstantVoltagePercent) / ConstantVoltageUs));
            return std::min(p, 99); // 100 is the charger's word, not the voltage's.
        }
        constantVoltageSince_ = -1;
        return batteryPercentFromMv(mv);
    }
private:
    TimeUs constantVoltageSince_ = -1;
};
}
