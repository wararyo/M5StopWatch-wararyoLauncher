#include "ImuWake.h"
#include <M5Unified.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdio>
namespace launcher {
namespace {
// The feature-engine image M5Unified uploads for its own BMI270 driver. It is
// byte-identical to Bosch BMI270_SensorAPI's default bmi270.c image (checked
// against 41129fc), the one that carries wrist-wear wake-up. Included here
// because M5Unified gives it internal linkage.
#include <utility/imu/BMI270_config.inl>

constexpr uint8_t Imu = 0x68;  // SDO pulled down by R47.
constexpr uint32_t ImuHz = 400000;

// BMI270 registers (datasheet 5.2) and the feature layout of the image above.
enum : uint8_t {
    ChipId = 0x00, IntStatus0 = 0x1C, InternalStatus = 0x21, FeatPage = 0x2F, Features = 0x30,
    AccConf = 0x40, Int1IoCtrl = 0x53, IntLatch = 0x55, Int1MapFeat = 0x56, InitCtrl = 0x59,
    InitAddr0 = 0x5B, InitData = 0x5E, PwrConf = 0x7C, PwrCtrl = 0x7D, Cmd = 0x7E,
};
constexpr uint8_t WristPage = 7, WristByte = 0, WristEnable = 0x10;
// GEN_SET_1 axis map: per engine axis, the sensor axis it reads (0 x, 1 y,
// 2 z) and whether it is negated. [1:0] x, [2] x sign, [4:3] y, [5] y sign,
// [7:6] z; z's sign is bit 0 of the next byte. The identity is 0x88.
constexpr uint8_t AxisMapPage = 1, AxisMapByte = 4;
constexpr uint8_t WatchAxes = 0x01 | (0x00 << 3) | 0x20 | (0x02 << 6);  // x=+y, y=-x, z=+z
constexpr uint8_t WristStatus = 0x08;  // INT_STATUS_0 and INT1_MAP_FEAT
// Low-power accelerometer (filter_perf 0), 4-sample average, 50Hz: the
// feature engine runs at 50Hz.
constexpr uint8_t AccLowPower50Hz = 0x27;

bool imuWrite(uint8_t reg, uint8_t value) { return M5.In_I2C.writeRegister8(Imu, reg, value, ImuHz); }
bool imuRead(uint8_t reg, uint8_t* data, size_t length) { return M5.In_I2C.readRegister(Imu, reg, data, length, ImuHz); }

bool loadFeatureEngine() {
    uint8_t id = 0;
    if (!imuRead(ChipId, &id, 1) || id != 0x24) {
        std::printf("[ImuWake] BMI270 not found at 0x%02x (chip id 0x%02x)\n", Imu, id);
        return false;
    }
    imuWrite(Cmd, 0xB6);  // Soft reset; the chip needs 2ms before the next access.
    vTaskDelay(pdMS_TO_TICKS(3));
    // Advanced power save off for the upload: in it every write needs 450us.
    if (!imuWrite(PwrConf, 0x00)) return false;
    vTaskDelay(pdMS_TO_TICKS(1));
    if (!imuWrite(InitCtrl, 0x00)) return false;
    constexpr size_t Chunk = 64;  // Even: the address counts 16-bit words.
    for (size_t index = 0; index < sizeof(bmi270_config_file); index += Chunk) {
        const uint8_t address[2] = {uint8_t((index / 2) & 0x0F), uint8_t((index / 2) >> 4)};
        if (!M5.In_I2C.writeRegister(Imu, InitAddr0, address, sizeof(address), ImuHz) ||
            !M5.In_I2C.writeRegister(Imu, InitData, bmi270_config_file + index, Chunk, ImuHz)) return false;
    }
    if (!imuWrite(InitCtrl, 0x01)) return false;
    // The datasheet allows 20ms for the engine to start.
    uint8_t status = 0;
    for (int retry = 0; retry < 10 && (status & 0x0F) != 0x01; ++retry) {
        vTaskDelay(pdMS_TO_TICKS(5));
        imuRead(InternalStatus, &status, 1);
    }
    if ((status & 0x0F) != 0x01) {
        std::printf("[ImuWake] feature engine did not start: internal_status=0x%02x\n", status);
        return false;
    }
    return true;
}

bool enableWristWear() {
    // Accelerometer on: the feature engine reads it.
    if (!imuWrite(AccConf, AccLowPower50Hz) || !imuWrite(PwrCtrl, 0x04)) return false;
    // The gesture assumes a watch frame (datasheet Figure 3): x to 3 o'clock,
    // y to 12, z out of the dial. The StopWatch is worn with the screen's top
    // at 12 o'clock on the left wrist, and its sensor has +x towards the
    // screen's bottom and +y towards its right (UserDemo's IMU app), so the
    // engine gets x = +y and y = -x. Only the engine's input changes; DATA
    // registers keep the sensor frame.
    uint8_t axes[2]{};
    if (!imuWrite(FeatPage, AxisMapPage) || !imuRead(uint8_t(Features + AxisMapByte), axes, sizeof(axes)))
        return false;
    std::printf("[ImuWake] axis map was 0x%02x 0x%02x\n", axes[0], axes[1]);
    axes[0] = WatchAxes;
    axes[1] &= uint8_t(~0x01);  // z sign: positive
    if (!M5.In_I2C.writeRegister(Imu, uint8_t(Features + AxisMapByte), axes, sizeof(axes), ImuHz)) return false;
    // The feature page is written whole, so the other features' inputs stay
    // at the image's defaults, wrist-wear wake-up's thresholds among them.
    uint8_t page[16]{};
    if (!imuWrite(FeatPage, WristPage) || !imuRead(Features, page, sizeof(page))) return false;
    page[WristByte] |= WristEnable;
    if (!M5.In_I2C.writeRegister(Imu, Features, page, sizeof(page), ImuHz)) return false;
    // INT1: output enabled, push-pull, active high (it drives Q7's gate), and
    // latched until INT_STATUS_0 is read, so M5PM1's scan cannot miss it.
    if (!imuWrite(Int1IoCtrl, 0x0A) || !imuWrite(IntLatch, 0x01) || !imuWrite(Int1MapFeat, WristStatus))
        return false;
    uint8_t clear[2];
    imuRead(IntStatus0, clear, sizeof(clear));
    // Advanced power save last: every register write above would need 450us in it.
    uint8_t pwrConf = 0, status = 0;
    if (!imuRead(PwrConf, &pwrConf, 1) || !imuWrite(PwrConf, uint8_t(pwrConf | 0x01))) return false;
    // The engine flags a map it cannot use once a feature is enabled.
    if (imuRead(InternalStatus, &status, 1) && (status & 0x20)) {
        std::printf("[ImuWake] axis map rejected: internal_status=0x%02x\n", status);
        return false;
    }
    return true;
}

}

bool beginImuWake() {
    const bool imu = loadFeatureEngine() && enableWristWear();
    std::printf("[ImuWake] imu=%s\n", imu ? "ok" : "FAILED");
    return imu;
}

ImuWakeStatus readImuWake() {
    ImuWakeStatus s{};
    uint8_t imu = 0;
    s.read = imuRead(IntStatus0, &imu, 1);
    s.wrist = s.read && (imu & WristStatus);
    return s;
}
}
