#include "Pm1Irq.h"
#include <M5Unified.h>
#include <driver/gpio.h>
#include <cstdio>
namespace launcher {
namespace {
constexpr uint8_t Pm1 = 0x6E;
constexpr uint32_t Pm1Hz = 100000;
constexpr gpio_num_t IrqPin = GPIO_NUM_12;  // G12_PY_IRQ = M5PM1 G1

// M5PM1 registers (M5PM1 datasheet 5).
enum : uint8_t {
    GpioMode = 0x10, GpioDrv = 0x13, GpioFunc0 = 0x16,
    IrqStatus1 = 0x40, IrqStatus2 = 0x41, IrqMask1 = 0x43, IrqMask2 = 0x44, IrqMask3 = 0x45,
};
// IRQ_STATUS2 and its mask: [1] 5VIN removed, [0] 5VIN added.
constexpr uint8_t VinEvents = 0x03;

bool pm1Write(uint8_t reg, uint8_t value) { return M5.In_I2C.writeRegister8(Pm1, reg, value, Pm1Hz); }
bool pm1Read(uint8_t reg, uint8_t& value) { return M5.In_I2C.readRegister(Pm1, reg, &value, 1, Pm1Hz); }
// Read-modify-write: M5Unified configures G2 in the same registers.
bool pm1Update(uint8_t reg, uint8_t clear, uint8_t set) {
    uint8_t value = 0;
    return pm1Read(reg, value) && pm1Write(reg, uint8_t((value & ~clear) | set));
}
bool clearStatus() {
    bool ok = true;
    for (uint8_t i = 0; i < 3; ++i) ok = pm1Write(uint8_t(IrqStatus1 + i), 0x00) && ok;
    return ok;
}
}

bool beginPm1Irq(bool imu) {
    gpio_config_t io{};
    io.pin_bit_mask = 1ULL << IrqPin;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;  // Harmless with push-pull; keeps an unconfigured PM1 high.
    gpio_config(&io);
    // G0 only with the IMU behind it; G2 is the charge status, G3 and G4 the
    // charge programming and the port. Of the power events only 5VIN: 5VINOUT
    // is the port, and battery events need charging disabled. The button has
    // its own reader.
    bool ok = pm1Write(IrqMask1, imu ? 0x1E : 0x1F) && pm1Write(IrqMask2, uint8_t(0x3F & ~VinEvents)) &&
              pm1Write(IrqMask3, 0x07);
    // G0: a plain input (R46 pulls it up), so it is scanned.
    if (imu) ok = ok && pm1Update(GpioFunc0, 0x03, 0x00) && pm1Update(GpioMode, 0x01, 0x00);
    // G1: push-pull, since M5PM1 is the only driver of G12_PY_IRQ; then its
    // IRQ function.
    ok = ok && pm1Update(GpioDrv, 0x02, 0x00) && clearStatus();
    ok = ok && pm1Update(GpioFunc0, 0x0C, 0x04);
    // Scanning starts with the IRQ function; a change seen while it was being
    // set up is not an event, and the caller reads VBUS afterwards anyway.
    ok = clearStatus() && ok;
    std::printf("[Pm1Irq] imu=%d result=%s irq=%s\n", int(imu), ok ? "ok" : "FAILED",
                pm1IrqActive() ? "low" : "high");
    return ok;
}

Pm1IrqStatus clearPm1Irq() {
    Pm1IrqStatus s{};
    pm1Read(IrqStatus2, s.power);
    s.cleared = clearStatus();
    return s;
}

bool pm1IrqActive() { return gpio_get_level(IrqPin) == 0; }
}
