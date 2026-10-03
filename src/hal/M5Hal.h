#pragma once
#include "Hal.h"
#include "power/BatteryCurve.h"
namespace launcher {
class M5Hal final : public Hal {
public:
    TimeUs now() override;
    InputSnapshot sampleInput() override;
    UsbState sampleUsb() override;
    void setScreenOff(bool off) override;
    void waitUs(TimeUs delay) override;
    bool inputPending() override;
    bool takeWristWake() override;
    bool usbEvents() const override { return inputWake_ && pm1Irq_; }
    bool takeUsbEvent() override;
    void setLightSleepAllowed(bool allowed) override;
    TimeUs panelShowsAt() const override { return panelShowsAt_; }
    bool setStatusLed(bool on) override;
    // Call on the UI task: it is the task the interrupts notify. Sets up the
    // IMU's wrist-wear wake-up and M5PM1's IRQ output first, since that line
    // is one of them.
    void beginInputWake();
    bool readRtc(CivilTime& utc) override;
    bool writeRtc(const CivilTime& utc) override;
    void setUtcClock(int64_t unixSeconds) override;
    int64_t utcClockUs() override;
    BatteryState sampleBattery() override;
    void setBrightness(int level) override;
private:
    bool inputWake_ = false, pending_ = false, imuWake_ = false, imuRetry_ = false, pm1Irq_ = false;
    bool wrist_ = false, usbEvent_ = false; // Taken by the calls above.
    TimeUs panelShowsAt_ = 0;
    void servicePm1Irq();
    // USB power and charging as of the last valid reads, for the estimator.
    bool usbPowered_ = false, chargingKnown_ = false, charging_ = false;
    TimeUs chargingChangedAt_ = -1;
    BatteryEstimator battery_;
};
// Dynamic frequency scaling between the two limits, and automatic light sleep
// while setLightSleepAllowed(true) (work 8-5).
void beginPowerManagement(int maxMhz, int minMhz);
// M5IOE1 sleeps once the shared I2C bus has been quiet for a second, holding
// its outputs, and any traffic on the bus wakes it in about 2ms; an access of
// its own that wakes it fails. M5.begin() turns the sleep off. Nothing reads or
// writes the chip after it, so nothing has to wake it; the next boot's M5GFX
// probe retries for 200ms, and turns the sleep off again.
void beginIoe1IdleSleep();
void beginRuntimeDiagnostics();
void runtimeDiagnostics();
}
