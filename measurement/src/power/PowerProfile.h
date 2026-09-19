#pragma once

#include "ui/Renderer.h"

namespace power {

/// Scripted power measurement run, compiled in only with
/// -DLAUNCHER_POWER_PROFILE.
///
/// A USB tester on VBUS cannot tell the board's consumption apart from battery
/// charging, and its instantaneous reading is too coarse and too jittery to
/// compare 20 mA against 40 mA. So this holds each workload for a fixed,
/// announced duration and lets the tester's mAh accumulator do the integrating:
///
///     average current [mA] = delta mAh * 3600 / hold seconds
///
/// The cases are arranged so the three terms of the power budget can be
/// separated rather than measured as one lump:
///
///   * baseline        - screen off, nothing drawn
///   * panel emission  - identical redraw work, black screen versus white
///   * render work     - identical content, redraw forced at 10/20/30 fps
///
/// Each case also reports the render duty cycle, which is immune to charging
/// current and to USB, and is perfectly repeatable. Calibrate it against two or
/// three tester readings and later workloads can be judged without the meter.
///
/// Before running: charge the battery to full. While the charger is active the
/// tester reads charging current, not consumption. Note also that the USB
/// console keeps the USB PHY awake, so the screen-off figure measured this way
/// is an upper bound; the real idle draw has to be confirmed on battery.
void runPowerProfile(ui::Renderer& renderer, M5GFX& display, uint32_t holdSeconds = 120);

}  // namespace power
