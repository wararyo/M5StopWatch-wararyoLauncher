#include "DigitalWatchFace.h"
#include <cstdio>
#include <cstring>

namespace ui {
namespace {
constexpr uint16_t Black = 0x0000;
constexpr uint16_t White = 0xffff;
constexpr uint16_t Muted = 0xad75;
constexpr uint16_t Lime = 0xb7e0;
constexpr const char* Weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
constexpr const char* Months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                  "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

const lgfx::IFont* const TimeFont = &fonts::FreeSansBold24pt7b;
constexpr float TimeScale = 2.15f;
constexpr int TimeCenterY = 270;
constexpr int SpriteMargin = 8;
}

bool DigitalWatchFace::begin(Gfx& gfx) {
    gfx.setFont(TimeFont);
    gfx.setTextSize(TimeScale);
    const int w = gfx.textWidth("00:00") + SpriteMargin;
    const int h = gfx.fontHeight() + SpriteMargin;
    gfx.setTextSize(1.0f);

    timeText_.setPsram(false);  // internal SRAM, not the PSRAM heap
    timeText_.setColorDepth(16);
    timeTextReady_ = timeText_.createSprite(w, h) != nullptr;

    std::printf("[WatchFace] time cache %dx%d = %d bytes (%s)\n", w, h, w * h * 2,
                timeTextReady_ ? "internal SRAM" : "alloc failed, drawing direct");
    return true;
}

void DigitalWatchFace::drawTime(Gfx& gfx, const char* text, int centerY) {
    if (!timeTextReady_) {
        gfx.setTextDatum(middle_center);
        gfx.setTextColor(White, Black);
        gfx.setFont(TimeFont);
        gfx.setTextSize(TimeScale);
        gfx.drawString(text, 234, centerY);
        gfx.setTextSize(1.0f);
        return;
    }

    if (std::strncmp(text, cachedTime_, sizeof(cachedTime_) - 1) != 0) {
        std::snprintf(cachedTime_, sizeof(cachedTime_), "%s", text);
        timeText_.fillScreen(Black);
        timeText_.setTextDatum(middle_center);
        timeText_.setTextColor(White, Black);
        timeText_.setFont(TimeFont);
        timeText_.setTextSize(TimeScale);
        timeText_.drawString(cachedTime_, timeText_.width() / 2, timeText_.height() / 2);
        timeText_.setTextSize(1.0f);
    }
    timeText_.pushSprite(&gfx, 234 - timeText_.width() / 2, centerY - timeText_.height() / 2);
}

void DigitalWatchFace::draw(Gfx& c, const WatchData& data, int y) {
    char text[32];
    const int screenHeight = c.height();
    // Skip anything that has scrolled past an edge. Without this the whole face
    // is still rasterised and clipped away while the app list is being dragged
    // in over it.
    auto onScreen = [screenHeight](int centerY, int halfHeight) {
        return centerY + halfHeight > 0 && centerY - halfHeight < screenHeight;
    };

    c.setTextDatum(middle_center);

    if (onScreen(y + 86, 32)) {
        std::snprintf(text, sizeof(text), data.charging ? "+ %d%%" : "%d%%",
                      data.batteryPercent < 0 ? 0 : data.batteryPercent);
        c.setTextColor(Lime, Black);
        c.setFont(&fonts::FreeSans18pt7b);
        c.drawString(text, 234, y + 86);
    }

    if (onScreen(y + 145, 32)) {
        c.setTextColor(Muted, Black);
        c.setFont(&fonts::FreeSans18pt7b);
        if (data.timeValid) {
            std::snprintf(text, sizeof(text), "%s, %s %02d", Weekdays[data.localTime.tm_wday],
                          Months[data.localTime.tm_mon], data.localTime.tm_mday);
        } else {
            std::snprintf(text, sizeof(text), "SET TIME");
        }
        c.drawString(text, 234, y + 145);
    }

    const int timeHalfHeight = timeTextReady_ ? timeText_.height() / 2 + 4 : 80;
    if (onScreen(y + TimeCenterY, timeHalfHeight)) {
        if (data.timeValid) {
            std::snprintf(text, sizeof(text), "%02d:%02d", data.localTime.tm_hour,
                          data.localTime.tm_min);
        } else {
            std::snprintf(text, sizeof(text), "--:--");
        }
        drawTime(c, text, y + TimeCenterY);
    }

    // Colour is passed explicitly: the four dots are lime in the reference
    // design, and with the culling above there is no reliable preceding draw
    // to inherit a colour from.
    if (onScreen(y + 382, 26)) {
        c.fillRect(218, y + 367, 10, 10, Lime);
        c.fillRect(238, y + 367, 10, 10, Lime);
        c.fillRect(218, y + 387, 10, 10, Lime);
        c.fillRect(238, y + 387, 10, 10, Lime);
    }

    if (onScreen(y + 425, 20)) {
        c.setTextDatum(middle_center);
        c.setTextColor(Muted, Black);
        c.setFont(&fonts::FreeSans12pt7b);
        c.drawString("APPS", 234, y + 425);
    }
}

uint32_t DigitalWatchFace::nextUpdateDelayMs(const WatchData& data) const {
    if (!data.timeValid) return 1000;
    return static_cast<uint32_t>((60 - data.localTime.tm_sec) * 1000);
}

}  // namespace ui
