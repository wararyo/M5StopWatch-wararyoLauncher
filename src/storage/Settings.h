#pragma once
#include <cstddef>
#include <cstdint>
namespace launcher {
// The settings the launcher persists. Kept apart from the store so the display
// model can carry a copy without depending on NVS.
struct Settings {
    uint8_t brightness = 90;
    uint16_t screenOffSec = 30;
};
// Below this the panel stops being readable indoors, so the editor cannot go
// there (plan.md 5.4). The step divides the range exactly into 16 stops.
constexpr uint8_t BrightnessMin = 30, BrightnessMax = 255, BrightnessStep = 15;
constexpr uint16_t ScreenOffChoices[] = {15, 30, 60, 180};
constexpr int ScreenOffChoiceCount = int(sizeof(ScreenOffChoices) / sizeof(ScreenOffChoices[0]));
constexpr bool validBrightness(int value) { return value >= BrightnessMin && value <= BrightnessMax; }
constexpr bool validScreenOff(int seconds) {
    for (const auto choice : ScreenOffChoices) if (int(choice) == seconds) return true;
    return false;
}
constexpr bool validSettings(const Settings& s) {
    return validBrightness(s.brightness) && validScreenOff(s.screenOffSec);
}
// Index of a screen-off value in the candidate list, or 0 when it is not one.
constexpr int screenOffIndex(int seconds) {
    for (int i = 0; i < ScreenOffChoiceCount; ++i) if (int(ScreenOffChoices[i]) == seconds) return i;
    return 0;
}
}
