#include "Renderer.h"
#include <cmath>

namespace ui {
namespace {
constexpr uint16_t Black = 0x0000;
constexpr uint16_t White = 0xffff;
constexpr uint16_t Muted = 0xad75;
constexpr uint16_t Lime = 0xb7e0;

struct AppItem { const char* label; uint16_t color; const char* icon; };
constexpr AppItem Items[] = {
    {"STOPWATCH", 0x349f, "S"}, {"SETTINGS", 0x632c, "*"}, {"EXTERNAL 1", 0x2e17, "1"}};
}

Renderer::Renderer(M5GFX& display) : display_(display) {}

bool Renderer::begin() {
    if (display_.width() <= 0 || display_.height() <= 0) return false;
    return watchFace_.begin(display_);
}

void Renderer::drawAppList(const app::State& state, int yOffset) {
    const int centerY = 226;
    for (int i = 0; i < app::LauncherApp::AppCount; ++i) {
        const int y = yOffset + centerY + i * 94 - static_cast<int>(state.listScroll);
        if (y < -60 || y > display_.height() + 60) continue;
        const bool selected = i == state.selected;
        display_.fillCircle(88, y, selected ? 37 : 32, Items[i].color);
        display_.setTextDatum(middle_center);
        display_.setTextColor(White, Items[i].color);
        display_.setFont(&fonts::FreeSansBold18pt7b);
        display_.drawString(Items[i].icon, 88, y);
        display_.setTextDatum(middle_left);
        display_.setTextColor(selected ? Lime : White, Black);
        display_.setFont(selected ? &fonts::FreeSansBold18pt7b : &fonts::FreeSans18pt7b);
        display_.drawString(Items[i].label, 143, y);
    }
    const int hintY = yOffset + 454;
    if (hintY > 0 && hintY - 30 < display_.height()) {
        display_.setTextDatum(bottom_center);
        display_.setTextColor(Muted, Black);
        display_.setFont(&fonts::FreeSans9pt7b);
        display_.drawString("A NEXT   B OPEN   A+B HOME", 234, hintY);
    }
}

void Renderer::draw(const app::State& state, const WatchData& watch) {
    const int screenHeight = display_.height();
    const int progress = static_cast<int>(state.transition * screenHeight);
    const int watchY = -progress;
    const int listY = screenHeight - progress;

    // Everything between startWrite() and endWrite() lands in the panel's PSRAM
    // framebuffer. Without this bracket the panel's auto-display would flush
    // once per drawing call.
    display_.startWrite();
    display_.fillScreen(Black);

    // Each layer is one screen tall. Once a layer has scrolled fully past an
    // edge it contributes nothing, so it is skipped rather than drawn and
    // clipped. This is what keeps the watch face out of the frame budget while
    // the app list is open, which is the case that was missing frames.
    if (watchY + screenHeight > 0 && watchY < screenHeight) {
        watchFace_.draw(display_, watch, watchY);
    }
    if (listY + screenHeight > 0 && listY < screenHeight) {
        drawAppList(state, listY);
    }

    if (state.toast) {
        display_.fillRoundRect(58, 375, 352, 54, 18, 0x2104);
        display_.drawRoundRect(58, 375, 352, 54, 18, Muted);
        display_.setTextDatum(middle_center);
        display_.setTextColor(White, 0x2104);
        display_.setFont(&fonts::FreeSans12pt7b);
        display_.drawString(state.toast, 234, 402);
    }
    display_.endWrite();
}

}  // namespace ui
