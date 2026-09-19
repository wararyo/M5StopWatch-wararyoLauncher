#include <M5Unified.h>
#include <cstdio>
#include "app/LauncherApp.h"
#include "input/InputController.h"
#include "power/PowerManager.h"
#include "time/TimeService.h"
#include "ui/Renderer.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
uint32_t nowMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
}

extern "C" void app_main() {
    auto cfg = M5.config();
    cfg.internal_imu = false;
    cfg.internal_rtc = true;
    cfg.internal_mic = false;
    cfg.internal_spk = false;
    cfg.output_power = false;
    cfg.clear_display = true;
    M5.begin(cfg);
    M5.Display.setBrightness(90);

    std::printf("[Launcher] board=%d display=%dx%d cpu=240MHz prototype\n",
                static_cast<int>(M5.getBoard()), static_cast<int>(M5.Display.width()),
                static_cast<int>(M5.Display.height()));

    clock_service::TimeService time;
    std::printf("[Clock] RTC=%s timezone=JST-9\n", time.begin() ? "valid" : "invalid");
    ui::Renderer renderer(M5.Display);
    if (!renderer.begin()) {
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextColor(TFT_RED);
        M5.Display.drawCenterString("Display init failed", 234, 220);
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    app::LauncherApp app;
    input::InputController input;
    power::PowerManager power;
    power.begin(nowMs());

    bool redraw = true;
    bool dragging = false;
    uint32_t nextClockUpdate = 0;
    uint32_t nextBatteryUpdate = 0;
    uint32_t nextFrameReport = 5000;
    uint32_t frameCount = 0;
    uint32_t missedFrames = 0;
    int64_t maximumFrameUs = 0;
    ui::WatchData watch{};
    auto last = xTaskGetTickCount();

    while (true) {
        M5.update();
        const uint32_t now = nowMs();
        const bool wasOff = power.screenOff();
        const auto touch = M5.Touch.getDetail();
        const bool rawActivity = M5.BtnA.isPressed() || M5.BtnB.isPressed() || touch.isPressed();
        if (power.update(now, rawActivity, M5.Power.getVBUSVoltage() > 4000)) redraw = true;
        const auto events = input.update(now, touch, wasOff && touch.wasPressed());

        if (events.home) { app.home(); redraw = true; }
        else {
            if (events.next) { app.next(now); redraw = true; }
            if (events.decide) { app.decide(now); redraw = true; }
            if (events.touchStarted) dragging = true;
            if (events.touchMoved) {
                if (app.state().screen == app::Screen::Watch) app.dragWatch(events.dragTotalY);
                else app.dragList(events.dragDeltaY);
                redraw = true;
            }
            if (events.touchEnded) {
                app.finishDrag(events.dragTotalY);
                dragging = false;
                redraw = true;
            }
        }
        if (app.tick(now)) redraw = true;

        if (now >= nextClockUpdate) {
            watch.timeValid = time.now(watch.localTime);
            nextClockUpdate = now + (watch.timeValid ?
                static_cast<uint32_t>((60 - watch.localTime.tm_sec) * 1000) : 1000);
            redraw = true;
        }
        if (now >= nextBatteryUpdate) {
            watch.batteryPercent = M5.Power.getBatteryLevel();
            watch.charging = M5.Power.isCharging();
            nextBatteryUpdate = now + 30000;
            redraw = true;
        }
        if (redraw && !power.screenOff()) {
            const int64_t start = esp_timer_get_time();
            renderer.draw(app.state(), watch);
            const int64_t elapsed = esp_timer_get_time() - start;
            ++frameCount;
            if (elapsed > 33333) ++missedFrames;
            if (elapsed > maximumFrameUs) maximumFrameUs = elapsed;
            redraw = false;
        }
        if (now >= nextFrameReport) {
            std::printf("[FrameStats] frames=%lu missed_33ms=%lu max_us=%lld dragging=%d\n",
                        static_cast<unsigned long>(frameCount),
                        static_cast<unsigned long>(missedFrames), maximumFrameUs, dragging);
            frameCount = missedFrames = 0;
            maximumFrameUs = 0;
            nextFrameReport = now + 5000;
        }
        vTaskDelayUntil(&last, pdMS_TO_TICKS(dragging ? 16 : 10));
    }
}
