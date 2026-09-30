#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace launcher {
// Wakes the UI task on a button press, a touch-controller interrupt or (with
// `imuIrq`) the IMU's wrist-wear wake-up routed through M5PM1, so the
// runtime can wait for its next deadline instead of polling every 10ms while
// nothing is touched (docs/task8/plan.md 8-4), and ends automatic light sleep
// (8-5). Ported from MuteHid's InputWake.
// False when the interrupts could not be installed; the caller must then keep
// polling, since no press would ever end a long wait. `imuIrq` only once the
// route is set up: a line left low would end every light sleep at once.
bool beginInputWake(TaskHandle_t task, bool imuIrq);
// Interrupts are level-triggered and disable themselves on firing; call before
// each wait to re-enable the pins that are back at their idle (high) level.
void rearmInputWake();
}
