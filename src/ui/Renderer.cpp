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
    return display_.width() > 0 && display_.height() > 0;
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
    display_.setTextDatum(bottom_center);
    display_.setTextColor(Muted, Black);
    display_.setFont(&fonts::FreeSans9pt7b);
    display_.drawString("A NEXT   B OPEN   A+B HOME", 234, yOffset + 454);
}

void Renderer::draw(const app::State& state, const WatchData& watch) {
    // Everything between startWrite() and endWrite() lands in the panel's PSRAM
    // framebuffer. Without this bracket the panel's auto-display would flush
    // once per drawing call.
    display_.startWrite();
    display_.fillScreen(Black);
    const int progress = static_cast<int>(state.transition * display_.height());
    watchFace_.draw(display_, watch, -progress);
    drawAppList(state, display_.height() - progress);
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
