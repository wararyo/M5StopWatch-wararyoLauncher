#include "M5Hal.h"
#include <M5Unified.h>
#include <esp_timer.h>
#include <driver/usb_serial_jtag.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
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
