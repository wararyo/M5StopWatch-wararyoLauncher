#include "DigitalWatchFace.h"
#include <cstdio>

namespace ui {
namespace {
constexpr uint16_t Black = 0x0000;
constexpr uint16_t White = 0xffff;
constexpr uint16_t Muted = 0xad75;
constexpr uint16_t Lime = 0xb7e0;
constexpr const char* Weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
constexpr const char* Months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                  "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
}

void DigitalWatchFace::draw(Gfx& c, const WatchData& data, int y) const {
    char text[32];
    c.setTextDatum(middle_center);

    std::snprintf(text, sizeof(text), data.charging ? "+ %d%%" : "%d%%",
                  data.batteryPercent < 0 ? 0 : data.batteryPercent);
    c.setTextColor(Lime, Black);
    c.setFont(&fonts::FreeSans18pt7b);
    c.drawString(text, 234, y + 86);

    c.setTextColor(Muted, Black);
    c.setFont(&fonts::FreeSans18pt7b);
    if (data.timeValid) {
        std::snprintf(text, sizeof(text), "%s, %s %02d", Weekdays[data.localTime.tm_wday],
                      Months[data.localTime.tm_mon], data.localTime.tm_mday);
    } else {
        std::snprintf(text, sizeof(text), "SET TIME");
    }
    c.drawString(text, 234, y + 145);

    c.setTextColor(White, Black);
    c.setFont(&fonts::FreeSansBold24pt7b);
    if (data.timeValid) {
        std::snprintf(text, sizeof(text), "%02d:%02d", data.localTime.tm_hour,
                      data.localTime.tm_min);
    } else {
        std::snprintf(text, sizeof(text), "--:--");
    }
    c.setTextSize(2.15f);
    c.drawString(text, 234, y + 270);
    c.setTextSize(1.0f);

    c.setTextColor(Lime, Black);
    c.fillRect(218, y + 367, 10, 10);
    c.fillRect(238, y + 367, 10, 10);
    c.fillRect(218, y + 387, 10, 10);
    c.fillRect(238, y + 387, 10, 10);
    c.setTextColor(Muted, Black);
    c.setFont(&fonts::FreeSans12pt7b);
    c.drawString("APPS", 234, y + 425);
}

uint32_t DigitalWatchFace::nextUpdateDelayMs(const WatchData& data) const {
    if (!data.timeValid) return 1000;
    return static_cast<uint32_t>((60 - data.localTime.tm_sec) * 1000);
}

}  // namespace ui

