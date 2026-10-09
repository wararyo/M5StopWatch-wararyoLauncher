#pragma once
#include <cstdint>
namespace launcher {
// BMI270 wrist-wear wake-up, delivered to the ESP32 as a level on GPIO12.
// On the StopWatch the IMU's INT1 drives the gate of Q7 (R50 pulls it down),
// which pulls M5PM1 G0 low; the line is shared with the RTC's nIRQ and pulled
// up by R46. M5PM1 reports the change on its IRQ output (hal/Pm1Irq.h). So
// INT1 is push-pull and active high, the opposite of a line the IMU would
// drive itself.
// The same feature engine counts steps (docs/task13/plan.md 2.1).
// Everything here is I2C: call it on the UI task, after M5.begin().
// Starting it soft-resets the IMU, so the step count begins again at 0.
struct ImuFeatures {
    bool wrist = false;   // Wrist-wear wake-up drives INT1.
    bool steps = false;   // The step counter counts, and readImuSteps reads it.
};
ImuFeatures beginImuWake();
// The steps counted since beginImuWake(). A burst read and nothing else,
// cheap enough in advanced power save. False when the read failed.
bool readImuSteps(uint32_t& steps);
struct ImuWakeStatus {
    // The IMU's status was read, which releases the latched INT1. Until it
    // is, G0 stays low and no further change can raise the IRQ: the caller
    // must read again although GPIO12 has gone high.
    bool read = false;
    bool wrist = false;   // The IMU's status says wrist-wear wake-up.
};
// Reads (and so clears) the IMU's status. Call after clearing M5PM1's: an
// event before the read is in it, and one after it changes G0 again and
// raises a fresh IRQ. The other order could clear the IRQ of an event that
// came between the two, with G0 then held low and never changing again. The
// latch's release is a level change on G0 too, so an empty status can follow.
ImuWakeStatus readImuWake();
}
