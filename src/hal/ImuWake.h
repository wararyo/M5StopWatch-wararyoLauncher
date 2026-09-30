#pragma once
namespace launcher {
// BMI270 wrist-wear wake-up, delivered to the ESP32 as a level on GPIO12.
// On the StopWatch the IMU's INT1 drives the gate of Q7 (R50 pulls it down),
// which pulls M5PM1 G0 low; the line is shared with the RTC's nIRQ and pulled
// up by R46. M5PM1 reports the change on its IRQ output G1 = G12_PY_IRQ and
// holds it low until its IRQ status is cleared. So INT1 is push-pull and
// active high, the opposite of a line the IMU would drive itself.
// Everything here is I2C: call it on the UI task, after M5.begin().
bool beginImuWake();
struct ImuWakeStatus {
    bool cleared = false; // M5PM1's IRQ status was cleared.
    // The IMU's status was read, which releases the latched INT1. Until it
    // is, G0 stays low and no further change can raise the IRQ: the caller
    // must call again although GPIO12 has gone high.
    bool read = false;
    bool wrist = false;   // The IMU's status says wrist-wear wake-up.
};
// Clears M5PM1's status, then reads (and so clears) the IMU's; call while
// GPIO12 is low, or again after `read` came back false. The latch's release
// is a level change on G0 too, so an empty status can follow.
ImuWakeStatus serviceImuWake();
// True while M5PM1 holds its IRQ output low.
bool imuWakeIrqActive();
}
