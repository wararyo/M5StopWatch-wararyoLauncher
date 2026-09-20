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
void M5Hal::draw(const ScreenModel& m, const UsbState& usb) {
    auto& d = M5.Display;
    d.fillScreen(TFT_BLACK);
    d.setTextSize(2);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    const int cx = d.width() / 2, cy = d.height() / 2;
    d.drawCenterString(m.screen == ScreenId::Home ? "Launcher home" : "Input check", cx, cy - 110);
    d.drawCenterString("TASK 2 DIAGNOSTICS", cx, cy - 80);
    if (m.screen == ScreenId::Home) {
        d.drawCenterString("A / B / tap: open", cx, cy - 35);
    } else {
        d.drawCenterString(m.selection == 0 ? "> Test event" : "Test event", cx, cy - 45);
        d.drawCenterString(m.selection == 1 ? "> Back" : "Back", cx, cy - 15);
        d.drawCenterString(m.lastEvent, cx, cy + 15);
    }
    char line[64];
    std::snprintf(line, sizeof(line), "Home count: %lu", static_cast<unsigned long>(m.homeCount));
    d.drawCenterString(line, cx, cy + 50);
    std::snprintf(line, sizeof(line), "VBUS:%s  USB:%s", !usb.vbusValid ? "?" : usb.powered() ? "on" : "off",
                  usb.dataConnected ? "PC" : "--");
    d.drawCenterString(line, cx, cy + 80);
    d.drawCenterString("A+B 600ms: home", cx, cy + 110);
    d.display();
    std::printf("[UI] screen=%s selection=%d event=%s homes=%lu vbus=%s usb=%d\n",
        m.screen == ScreenId::Home ? "home" : "input", m.selection, m.lastEvent,
        static_cast<unsigned long>(m.homeCount), !usb.vbusValid ? "unknown" : usb.powered() ? "on" : "off",
        int(usb.dataConnected));
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
