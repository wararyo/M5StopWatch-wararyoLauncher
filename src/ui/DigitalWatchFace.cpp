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
constexpr int CenterX = 234;
constexpr int BatteryY = 86;
constexpr int DateY = 145;
constexpr int TimeY = 270;
constexpr int DotsY = 367;
constexpr int AppsY = 425;
/// Slack around measured geometry, so an erase always covers what was inked.
constexpr int Margin = 8;

enum Slot { SlotBattery, SlotDate, SlotTime, SlotDots, SlotApps };

/// Box a middle_center string occupies, using the font currently set on `g`.
Rect centeredBox(Gfx& g, const char* text, int cx, int cy) {
    const int w = g.textWidth(text) + Margin;
    const int h = g.fontHeight() + Margin;
    return Rect{static_cast<int16_t>(cx - w / 2), static_cast<int16_t>(cy - h / 2),
                static_cast<int16_t>(w), static_cast<int16_t>(h)};
}
}  // namespace

bool DigitalWatchFace::begin(Gfx& gfx) {
    gfx.setFont(TimeFont);
    gfx.setTextSize(TimeScale);
    const int w = gfx.textWidth("00:00") + Margin;
    const int h = gfx.fontHeight() + Margin;
    gfx.setTextSize(1.0f);

    timeText_.setPsram(false);  // internal SRAM, not the PSRAM heap
    timeText_.setColorDepth(16);
    timeTextReady_ = timeText_.createSprite(w, h) != nullptr;

    std::printf("[WatchFace] time cache %dx%d = %d bytes (%s)\n", w, h, w * h * 2,
                timeTextReady_ ? "internal SRAM" : "alloc failed, drawing direct");
    return true;
}

void DigitalWatchFace::plan(FramePlan& frame, Gfx& gfx, const WatchData& data, int y) {
    const int screenW = gfx.width();
    const int screenH = gfx.height();
    originY_ = y;
    gfx.setTextSize(1.0f);

    std::snprintf(batteryText_, sizeof(batteryText_), data.charging ? "+ %d%%" : "%d%%",
                  data.batteryPercent < 0 ? 0 : data.batteryPercent);
    gfx.setFont(&fonts::FreeSans18pt7b);
    handles_[SlotBattery] = frame.add(
        battery_, clipToScreen(centeredBox(gfx, batteryText_, CenterX, y + BatteryY), screenW, screenH),
        hashString(batteryText_));

    if (data.timeValid) {
        std::snprintf(dateText_, sizeof(dateText_), "%s, %s %02d", Weekdays[data.localTime.tm_wday],
                      Months[data.localTime.tm_mon], data.localTime.tm_mday);
    } else {
        std::snprintf(dateText_, sizeof(dateText_), "SET TIME");
    }
    gfx.setFont(&fonts::FreeSans18pt7b);
    handles_[SlotDate] = frame.add(
        date_, clipToScreen(centeredBox(gfx, dateText_, CenterX, y + DateY), screenW, screenH),
        hashString(dateText_));

    if (data.timeValid) {
        std::snprintf(timeString_, sizeof(timeString_), "%02d:%02d", data.localTime.tm_hour,
                      data.localTime.tm_min);
    } else {
        std::snprintf(timeString_, sizeof(timeString_), "--:--");
    }
    if (timeTextReady_) {
        timeBox_ = Rect{static_cast<int16_t>(CenterX - timeText_.width() / 2),
                        static_cast<int16_t>(y + TimeY - timeText_.height() / 2),
                        static_cast<int16_t>(timeText_.width()),
                        static_cast<int16_t>(timeText_.height())};
    } else {
        gfx.setFont(TimeFont);
        gfx.setTextSize(TimeScale);
        timeBox_ = centeredBox(gfx, timeString_, CenterX, y + TimeY);
        gfx.setTextSize(1.0f);
    }
    handles_[SlotTime] =
        frame.add(time_, clipToScreen(timeBox_, screenW, screenH), hashString(timeString_));

    handles_[SlotDots] = frame.add(
        dots_, clipToScreen(Rect{218, static_cast<int16_t>(y + DotsY), 30, 30}, screenW, screenH),
        hashValue(0xd075u));

    gfx.setFont(&fonts::FreeSans12pt7b);
    handles_[SlotApps] = frame.add(
        appsLabel_, clipToScreen(centeredBox(gfx, "APPS", CenterX, y + AppsY), screenW, screenH),
        hashString("APPS"));
}

void DigitalWatchFace::paintTime(Gfx& gfx) {
    if (!timeTextReady_) {
        gfx.setTextDatum(middle_center);
        gfx.setTextColor(White, Black);
        gfx.setFont(TimeFont);
        gfx.setTextSize(TimeScale);
        gfx.drawString(timeString_, CenterX, timeBox_.y + timeBox_.h / 2);
        gfx.setTextSize(1.0f);
        return;
    }
    // Only re-rasterise when the digits themselves changed. Moving the clock,
    // as the app list drags it off screen, is then just a blit.
    if (std::strncmp(timeString_, cachedTime_, sizeof(cachedTime_) - 1) != 0) {
        std::snprintf(cachedTime_, sizeof(cachedTime_), "%s", timeString_);
        timeText_.fillScreen(Black);
        timeText_.setTextDatum(middle_center);
        timeText_.setTextColor(White, Black);
        timeText_.setFont(TimeFont);
        timeText_.setTextSize(TimeScale);
        timeText_.drawString(cachedTime_, timeText_.width() / 2, timeText_.height() / 2);
        timeText_.setTextSize(1.0f);
    }
    timeText_.pushSprite(&gfx, timeBox_.x, timeBox_.y);
}

void DigitalWatchFace::paint(Gfx& gfx, const FramePlan& frame) {
    gfx.setTextSize(1.0f);
    gfx.setTextDatum(middle_center);

    if (frame.shouldPaint(handles_[SlotBattery])) {
        gfx.setFont(&fonts::FreeSans18pt7b);
        gfx.setTextColor(Lime, Black);
        gfx.drawString(batteryText_, CenterX, originY_ + BatteryY);
    }
    if (frame.shouldPaint(handles_[SlotDate])) {
        gfx.setFont(&fonts::FreeSans18pt7b);
        gfx.setTextColor(Muted, Black);
        gfx.drawString(dateText_, CenterX, originY_ + DateY);
    }
    if (frame.shouldPaint(handles_[SlotTime])) {
        paintTime(gfx);
    }
    if (frame.shouldPaint(handles_[SlotDots])) {
        // Colour passed explicitly: the dots are lime in the reference design,
        // and elements paint in isolation so there is no preceding draw to
        // inherit a colour from.
        gfx.fillRect(218, originY_ + DotsY, 10, 10, Lime);
        gfx.fillRect(238, originY_ + DotsY, 10, 10, Lime);
        gfx.fillRect(218, originY_ + DotsY + 20, 10, 10, Lime);
        gfx.fillRect(238, originY_ + DotsY + 20, 10, 10, Lime);
    }
    if (frame.shouldPaint(handles_[SlotApps])) {
        gfx.setTextDatum(middle_center);
        gfx.setFont(&fonts::FreeSans12pt7b);
        gfx.setTextColor(Muted, Black);
        gfx.drawString("APPS", CenterX, originY_ + AppsY);
    }
}

uint32_t DigitalWatchFace::nextUpdateDelayMs(const WatchData& data) const {
    if (!data.timeValid) return 1000;
    return static_cast<uint32_t>((60 - data.localTime.tm_sec) * 1000);
}

}  // namespace ui
