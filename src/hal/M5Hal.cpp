#include "M5Hal.h"
#include <M5Unified.h>
#include <esp_timer.h>
#include <driver/usb_serial_jtag.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>
#include <cstdio>
#ifdef LAUNCHER_RUNTIME_DIAGNOSTICS
#include <esp_freertos_hooks.h>
#include <esp_rom_sys.h>
#include <atomic>
#endif
namespace launcher {
TimeUs M5Hal::now() { return esp_timer_get_time(); }
InputSnapshot M5Hal::sampleInput() {
    M5.update();
    const auto& t = M5.Touch.getDetail();
    return {M5.BtnA.isPressed(), M5.BtnB.isPressed(), t.isPressed(), t.x, t.y};
}
UsbState M5Hal::sampleUsb() {
    UsbState usb{};
    uint8_t bytes[2]{};
    // StopWatch PM1 VIN registers; never interpret a failed read as zero volts.
    usb.vbusValid = M5.getBoard() == m5::board_t::board_M5StopWatch &&
        M5.In_I2C.readRegister(0x6e, 0x24, bytes, sizeof(bytes), 100000);
    if (usb.vbusValid) usb.vbusMv = bytes[0] | (bytes[1] << 8);
    usb.dataConnected = usb_serial_jtag_is_connected();
    return usb;
}
void M5Hal::setScreenOff(bool off) {
    if (off) { M5.Display.setBrightness(0); M5.Display.sleep(); }
    else { M5.Display.wakeup(); M5.Display.setBrightness(90); }
    std::printf("[Power] screen=%s\n", off ? "off" : "on");
}
bool M5Hal::readRtc(CivilTime& utc) {
    if (!M5.Rtc.isEnabled()) return false;
    m5::rtc_datetime_t datetime{};
    if (!M5.Rtc.getDateTime(&datetime)) return false;
    utc = {datetime.date.year, datetime.date.month, datetime.date.date,
           datetime.time.hours, datetime.time.minutes, datetime.time.seconds};
    return true;
}
bool M5Hal::writeRtc(const CivilTime& utc) {
    if (!M5.Rtc.isEnabled()) return false;
    // setDateTime returns void, so reaching the chip is all this can report.
    // Whether the value stuck is decided by the read-back in TimeService::save.
    const int8_t weekday = int8_t(weekdayFromDays(daysFromCivil(utc.year, utc.month, utc.day)));
    M5.Rtc.setDateTime(m5::rtc_datetime_t{
        m5::rtc_date_t(int16_t(utc.year), int8_t(utc.month), int8_t(utc.day), weekday),
        m5::rtc_time_t(int8_t(utc.hour), int8_t(utc.minute), int8_t(utc.second))});
    return true;
}
void M5Hal::setUtcClock(int64_t unixSeconds) {
    const timeval tv{time_t(unixSeconds), 0};
    settimeofday(&tv, nullptr);
}
int64_t M5Hal::utcClockUs() {
    timeval tv{};
    gettimeofday(&tv, nullptr);
    return int64_t(tv.tv_sec) * 1000000 + tv.tv_usec;
}
BatteryState M5Hal::sampleBattery() {
    BatteryState battery{};
    const auto level = M5.Power.getBatteryLevel();
    // Out of range means the PMIC did not answer; leave it unknown.
    if (level >= 0 && level <= 100) battery.percent = int(level);
    battery.charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
    return battery;
}
void M5Hal::setBrightness(int level) {
    M5.Display.setBrightness(uint8_t(level < 0 ? 0 : level > 255 ? 255 : level));
}
void M5Hal::waitUs(TimeUs delay) {
    constexpr TimeUs tickUs = 1000000 / configTICK_RATE_HZ;
    const auto ticks = static_cast<TickType_t>((delay + tickUs - 1) / tickUs);
    vTaskDelay(ticks ? ticks : 1);
}
#ifdef LAUNCHER_RUNTIME_DIAGNOSTICS
namespace {
std::atomic<uint32_t> idleCount{0};
bool countIdle() { idleCount.fetch_add(1, std::memory_order_relaxed); return true; }
}
void beginRuntimeDiagnostics() {
    ESP_ERROR_CHECK(esp_register_freertos_idle_hook_for_cpu(countIdle, 1));
    std::printf("[RuntimeDiag] enabled: 40ms busy work each cycle; CPU1 idle hook\n");
}
void runtimeDiagnostics() {
    esp_rom_delay_us(40000);
    static TimeUs lastReport = esp_timer_get_time();
    const auto now = esp_timer_get_time();
    if (now - lastReport >= 5000000) {
        std::printf("[RuntimeDiag] idle_cpu1=%lu window_us=%lld\n",
            static_cast<unsigned long>(idleCount.exchange(0)), static_cast<long long>(now - lastReport));
        lastReport = now;
    }
}
#else
void beginRuntimeDiagnostics() {}
void runtimeDiagnostics() {}
#endif
}
