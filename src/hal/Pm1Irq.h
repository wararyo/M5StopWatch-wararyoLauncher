#pragma once
#include <cstdint>
namespace launcher {
// M5PM1's IRQ output G1 = G12_PY_IRQ, low while any of its IRQ status
// registers holds an event and released only once they are all cleared
// (M5PM1 datasheet 8). Two kinds of event are routed to it:
// - 5VIN added or removed (5VIN crossing 2.4V), i.e. USB power coming or
//   going, so VBUS need not be polled while the watch sleeps;
// - with `imu`, changes on G0, which the IMU's wrist-wear wake-up pulls low
//   (hal/ImuWake.h).
// Everything else (the other GPIOs, 5VINOUT, the battery, the button) stays
// masked. Everything here is I2C: call it on the UI task, after M5.begin().
bool beginPm1Irq(bool imu);
struct Pm1IrqStatus {
    bool cleared = false; // All three status registers were cleared.
    uint8_t power = 0;    // IRQ_STATUS2 as read before clearing it.
};
// Reads IRQ_STATUS2 for the log, then clears all three registers. Whatever is
// read afterwards (VBUS, the IMU's status) is newer than the events cleared.
Pm1IrqStatus clearPm1Irq();
// True while M5PM1 holds its IRQ output low.
bool pm1IrqActive();
}
