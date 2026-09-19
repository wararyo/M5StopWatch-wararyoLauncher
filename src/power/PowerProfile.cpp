#include "PowerProfile.h"

#ifdef LAUNCHER_POWER_PROFILE

#include <cstdio>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace power {
namespace {

/// The PM1 PMIC. Reads only: nothing in this file writes to it.
///
/// The dump was added to hunt for a register reporting current, which would
/// have let the board measure its own consumption instead of relying on a USB
/// tester. It does not have one. Sweeping 0x00..0x4f across every case below,
/// the only bytes that move at all are three 16-bit little-endian millivolt
/// readings:
///
///     0x20  system rail, about 3315 mV
///     0x22  battery                      (what M5Unified reports)
///     0x24  VBUS                         (what M5Unified reports)
///
/// 0x34 and 0x36 hold a static 500, most likely a current limit in mA. Every
/// other byte is identical whether the screen is off or the board is repainting
/// flat out, so there is nothing here to log against load. Power still has to
/// be measured externally.
constexpr uint8_t Pm1Address = 0x6e;
// 100 kHz: this PMIC does not answer at 400 kHz, which is what M5Unified uses
// for it as well.
constexpr uint32_t Pm1Freq = 100000;
constexpr uint8_t DumpFirst = 0x00;
constexpr uint8_t DumpLast = 0x4f;

constexpr uint16_t Black = 0x0000;
constexpr uint16_t White = 0xffff;

uint32_t nowMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

void dumpPmic(const char* when) {
    constexpr int Count = DumpLast - DumpFirst + 1;
    uint8_t regs[Count] = {};
    // Two bytes at a time, which is exactly how M5Unified reads battery and
    // VBUS millivolts from this PMIC. readRegister8 returns 0 on a failed
    // transaction without saying so, which on this chip looks identical to a
    // register full of zeroes, and a 32-byte burst is refused outright.
    int failures = 0;
    for (int i = 0; i + 1 < Count; i += 2) {
        if (!M5.In_I2C.readRegister(Pm1Address, static_cast<uint8_t>(DumpFirst + i), regs + i, 2,
                                    Pm1Freq)) {
            ++failures;
        }
    }
    if (failures) {
        std::printf("[PMIC] %-6s %d of %d pair reads failed\n", when, failures, Count / 2);
        if (failures == Count / 2) return;
    }

    char raw[3 * Count + 1];
    int at = 0;
    for (int i = 0; i < Count; ++i) {
        at += std::snprintf(raw + at, sizeof(raw) - at, "%02x ", regs[i]);
    }
    std::printf("[PMIC] %-6s 0x%02x..0x%02x: %s\n", when, DumpFirst, DumpLast, raw);

    // Battery and VBUS are 16-bit little endian millivolts at 0x22 and 0x24, so
    // anything reporting current is likely to be a 16-bit pair too. Printing
    // the pairs signed and unsigned makes a value that tracks load easy to spot.
    char pairs[Count / 2 * 16 + 1];
    at = 0;
    for (int i = 0; i + 1 < Count; i += 2) {
        const uint16_t u = static_cast<uint16_t>(regs[i] | (regs[i + 1] << 8));
        at += std::snprintf(pairs + at, sizeof(pairs) - at, "%02x:%u/%d ", DumpFirst + i, u,
                            static_cast<int>(static_cast<int16_t>(u)));
    }
    std::printf("[PMIC] %-6s 16bit LE (unsigned/signed): %s\n", when, pairs);
}

/// What one case does on each of its frames. Returns the microseconds spent
/// rendering, which is what the duty cycle is built from.
using StepFn = int64_t (*)(ui::Renderer&, M5GFX&, app::State&, ui::WatchData&, uint32_t frame);

struct Case {
    const char* name;
    uint8_t brightness;
    bool screenOn;
    /// Frames per second to attempt, or 0 to run flat out.
    uint16_t targetFps;
    StepFn step;
};

int64_t stepNothing(ui::Renderer&, M5GFX&, app::State&, ui::WatchData&, uint32_t) { return 0; }

int64_t stepWatchIdle(ui::Renderer& renderer, M5GFX&, app::State& state, ui::WatchData& data,
                      uint32_t) {
    // No invalidate: this is the launcher's real idle, where nothing changed
    // and the renderer paints nothing at all.
    state.transition = 0.0f;
    const int64_t start = esp_timer_get_time();
    renderer.draw(state, data);
    return esp_timer_get_time() - start;
}

int64_t stepWatchForced(ui::Renderer& renderer, M5GFX&, app::State& state, ui::WatchData& data,
                        uint32_t) {
    // Same pixels every frame, but repainted in full, so the only thing that
    // varies across the 10/20/30 fps cases is how much rendering happens.
    state.transition = 0.0f;
    const int64_t start = esp_timer_get_time();
    renderer.invalidate();
    renderer.draw(state, data);
    return esp_timer_get_time() - start;
}

int64_t stepListScroll(ui::Renderer& renderer, M5GFX&, app::State& state, ui::WatchData& data,
                       uint32_t frame) {
    // The heaviest thing the launcher actually does. Two pixels per frame is
    // roughly what a real drag produces.
    state.transition = 1.0f;
    state.listScroll = static_cast<float>((frame * 2) % 189);
    state.selected = static_cast<int>((state.listScroll + 47) / 94);
    const int64_t start = esp_timer_get_time();
    renderer.draw(state, data);
    return esp_timer_get_time() - start;
}

int64_t fillScreen(M5GFX& display, uint16_t color) {
    const int64_t start = esp_timer_get_time();
    display.startWrite();
    display.fillScreen(color);
    display.endWrite();
    return esp_timer_get_time() - start;
}

int64_t stepFillBlack(ui::Renderer&, M5GFX& display, app::State&, ui::WatchData&, uint32_t) {
    return fillScreen(display, Black);
}

int64_t stepFillWhite(ui::Renderer&, M5GFX& display, app::State&, ui::WatchData&, uint32_t) {
    return fillScreen(display, White);
}

ui::WatchData sampleData() {
    ui::WatchData data{};
    data.timeValid = true;
    data.localTime.tm_hour = 9;
    data.localTime.tm_min = 41;
    data.localTime.tm_wday = 6;
    data.localTime.tm_mon = 8;
    data.localTime.tm_mday = 19;
    data.batteryPercent = 82;
    return data;
}

const Case Cases[] = {
    // Baseline.
    {"screen-off",          0,   false,  0, stepNothing},
    // The launcher's real idle: content on screen, nothing repainted.
    {"watch-idle",          90,  true,   0, stepWatchIdle},
    // Render cost. Identical pixels, three redraw rates: the slope between
    // these three is the cost of one full repaint.
    {"watch-forced-10fps",  90,  true,  10, stepWatchForced},
    {"watch-forced-20fps",  90,  true,  20, stepWatchForced},
    {"watch-forced-30fps",  90,  true,  30, stepWatchForced},
    // The real worst case.
    {"list-scroll-max",     90,  true,   0, stepListScroll},
    // Panel emission. Same work, opposite content; the difference is what the
    // AMOLED costs to light up.
    {"fill-black-30fps",    90,  true,  30, stepFillBlack},
    {"fill-white-30fps",    90,  true,  30, stepFillWhite},
    // Brightness, measured on static content so only emission varies.
    {"watch-idle-bright10", 10,  true,   0, stepWatchIdle},
    {"watch-idle-bright255",255, true,   0, stepWatchIdle},
};

void runCase(const Case& c, ui::Renderer& renderer, M5GFX& display, uint32_t holdSeconds) {
    app::State state;
    ui::WatchData data = sampleData();

    if (c.screenOn) {
        display.wakeup();
        display.setBrightness(c.brightness);
        renderer.invalidate();
        renderer.draw(state, data);
    } else {
        display.setBrightness(0);
        display.sleep();
    }
    // Let the panel and the tester settle before the measured window opens.
    vTaskDelay(pdMS_TO_TICKS(2000));

    std::printf("\n[Power] >>> START %-22s brightness=%d hold=%lus  (mark the tester now)\n",
                c.name, static_cast<int>(c.brightness),
                static_cast<unsigned long>(holdSeconds));
    dumpPmic("start");

    const uint32_t begin = nowMs();
    const uint32_t deadline = begin + holdSeconds * 1000;
    const uint32_t periodMs = c.targetFps ? (1000u / c.targetFps) : 0u;
    uint32_t steps = 0;
    uint32_t missedPeriods = 0;
    int64_t renderUs = 0;
    int64_t worstUs = 0;
    bool dumpedMid = false;
    auto last = xTaskGetTickCount();

    while (true) {
        const uint32_t now = nowMs();
        if (static_cast<int32_t>(now - deadline) >= 0) break;

        // Every iteration counts, whether or not it painted: a case where the
        // renderer decides there is nothing to do should report a duty cycle
        // near zero rather than no samples at all.
        const int64_t spent = c.step(renderer, display, state, data, steps);
        ++steps;
        renderUs += spent;
        if (spent > worstUs) worstUs = spent;

        if (!dumpedMid && now - begin >= holdSeconds * 500) {
            dumpedMid = true;
            dumpPmic("mid");
        }

        // xTaskDelayUntil keeps the period measured from the previous wake, so
        // the achieved rate really is the target rate rather than the target
        // plus the render time. It returns false without blocking when the
        // deadline has already passed, which would starve the idle task and
        // trip the watchdog, so that case yields explicitly instead.
        if (!xTaskDelayUntil(&last, pdMS_TO_TICKS(periodMs ? periodMs : 5))) {
            ++missedPeriods;
            last = xTaskGetTickCount();
            vTaskDelay(1);
        }
    }

    const uint32_t elapsedMs = nowMs() - begin;
    const int duty = elapsedMs ? static_cast<int>(renderUs / (elapsedMs * 10)) : 0;
    std::printf("[Power] <<< END   %-22s  %lums  steps=%lu (%.1f/s, %lu over period)  "
                "duty=%d%%  avg=%lldus worst=%lldus\n",
                c.name, static_cast<unsigned long>(elapsedMs),
                static_cast<unsigned long>(steps),
                elapsedMs ? steps * 1000.0f / elapsedMs : 0.0f,
                static_cast<unsigned long>(missedPeriods), duty,
                steps ? renderUs / steps : 0, worstUs);
    std::printf("[Power]     battery=%dmV vbus=%dmV charging=%d  "
                "(average mA = delta mAh * 3600 / %lu)\n",
                static_cast<int>(M5.Power.getBatteryVoltage()),
                static_cast<int>(M5.Power.getVBUSVoltage()),
                M5.Power.isCharging() ? 1 : 0, static_cast<unsigned long>(holdSeconds));
}

}  // namespace

void runPowerProfile(ui::Renderer& renderer, M5GFX& display, uint32_t holdSeconds) {
    const int cases = sizeof(Cases) / sizeof(Cases[0]);
    const uint32_t totalSeconds = cases * (holdSeconds + 2);

    std::printf("\n[Power] ===== power profile: %d cases x %lus, about %lu min total =====\n",
                cases, static_cast<unsigned long>(holdSeconds),
                static_cast<unsigned long>((totalSeconds + 59) / 60));
    std::printf("[Power] Charge to full first. While charging, the tester reads the charger,\n");
    std::printf("[Power] not the board. charging=1 in any case below invalidates that case.\n");
    std::printf("[Power] Record the tester's mAh at each START and END line.\n");

    if (M5.Power.isCharging()) {
        std::printf("[Power] WARNING: charging right now. Results will be meaningless.\n");
    }
    dumpPmic("boot");

    for (int i = 0; i < cases; ++i) {
        runCase(Cases[i], renderer, display, holdSeconds);
    }

    std::printf("\n[Power] ===== done =====\n");
    display.wakeup();
    display.setBrightness(90);
    renderer.invalidate();
}

}  // namespace power

#endif  // LAUNCHER_POWER_PROFILE
