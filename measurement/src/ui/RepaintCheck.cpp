#include "RepaintCheck.h"

#ifdef LAUNCHER_BENCH

#include <cstdio>
#include <cstring>
#include "esp_heap_caps.h"
#include "esp_timer.h"

namespace ui {
namespace {

WatchData sampleData() {
    WatchData data{};
    data.timeValid = true;
    data.localTime.tm_hour = 9;
    data.localTime.tm_min = 41;
    data.localTime.tm_wday = 6;
    data.localTime.tm_mon = 8;
    data.localTime.tm_mday = 19;
    data.batteryPercent = 82;
    return data;
}

/// Average frame cost over `frames` calls, in microseconds.
void timeScenario(const char* name, Renderer& renderer, app::State& state, WatchData& data,
                  int frames, void (*step)(app::State&, WatchData&, int)) {
    int64_t total = 0, worst = 0;
    int painted = 0;
    for (int i = 0; i < frames; ++i) {
        step(state, data, i);
        const int64_t start = esp_timer_get_time();
        const bool did = renderer.draw(state, data);
        const int64_t elapsed = esp_timer_get_time() - start;
        if (!did) continue;
        ++painted;
        total += elapsed;
        if (elapsed > worst) worst = elapsed;
    }
    std::printf("[Bench] %-22s painted=%d/%d avg=%lldus worst=%lldus\n", name, painted, frames,
                painted ? total / painted : 0, worst);
}

void stepScroll(app::State& s, WatchData&, int i) {
    s.transition = 1.0f;
    s.listScroll = static_cast<float>((i * 2) % 189);
    s.selected = static_cast<int>((s.listScroll + 47) / 94);
}
void stepTransition(app::State& s, WatchData&, int i) {
    s.listScroll = 0.0f;
    s.selected = 0;
    s.transition = static_cast<float>((i * 4) % 101) / 100.0f;
}
void stepMinute(app::State& s, WatchData& d, int i) {
    s.transition = 0.0f;
    d.localTime.tm_min = 41 + (i & 1);
}
void stepIdle(app::State& s, WatchData&, int) { s.transition = 0.0f; }

}  // namespace

void runRepaintCheck(Renderer& renderer, M5GFX& display) {
    app::State state;
    WatchData data = sampleData();

    renderer.invalidate();
    timeScenario("app-list-scroll", renderer, state, data, 120, stepScroll);
    timeScenario("watch-to-list", renderer, state, data, 120, stepTransition);
    timeScenario("watch-minute-tick", renderer, state, data, 120, stepMinute);
    timeScenario("watch-idle", renderer, state, data, 120, stepIdle);

    const int w = display.width(), h = display.height();
    const size_t bytes = static_cast<size_t>(w) * h * 2;
    auto* incremental = static_cast<uint16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM));
    auto* reference = static_cast<uint16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM));
    if (!incremental || !reference) {
        std::printf("[Verify] could not allocate comparison buffers\n");
        heap_caps_free(incremental);
        heap_caps_free(reference);
        return;
    }

    int checks = 0, mismatches = 0;
    for (int variant = 0; variant < 24; ++variant) {
        // Dense sweep of the transition, which is where the watch face and the
        // app list trade places and their boxes can land on top of each other.
        state.transition = static_cast<float>(variant % 21) / 20.0f;
        // Walk there one frame at a time, so the history that partial
        // repainting depends on actually accumulates.
        for (int i = 0; i <= 17 + variant * 5; ++i) {
            state.listScroll = static_cast<float>((i * 3) % 189);
            state.selected = static_cast<int>((state.listScroll + 47) / 94);
            renderer.draw(state, data);
        }
        display.readRect(0, 0, w, h, incremental);

        renderer.invalidate();
        renderer.draw(state, data);
        display.readRect(0, 0, w, h, reference);

        ++checks;
        if (std::memcmp(incremental, reference, bytes) == 0) continue;

        ++mismatches;
        int x0 = w, y0 = h, x1 = -1, y1 = -1, diffs = 0, stale = 0, missing = 0;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const size_t k = static_cast<size_t>(y) * w + x;
                if (incremental[k] == reference[k]) continue;
                ++diffs;
                if (reference[k] == 0) ++stale;
                else if (incremental[k] == 0) ++missing;
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
        }
        std::printf("[Verify] MISMATCH transition=%.2f scroll=%d diffs=%d stale=%d missing=%d "
                    "box=(%d,%d)-(%d,%d)\n",
                    state.transition, static_cast<int>(state.listScroll), diffs, stale, missing,
                    x0, y0, x1, y1);
    }

    // Guard against readRect silently handing back nothing, which would make
    // every comparison pass for the wrong reason.
    size_t nonBlack = 0;
    for (size_t i = 0; i < bytes / 2; ++i) {
        if (reference[i]) ++nonBlack;
    }
    std::printf("[Verify] checks=%d mismatches=%d reference_nonblack_pixels=%u\n", checks,
                mismatches, static_cast<unsigned>(nonBlack));

    heap_caps_free(incremental);
    heap_caps_free(reference);
    renderer.invalidate();
}

}  // namespace ui

#endif  // LAUNCHER_BENCH
