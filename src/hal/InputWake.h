#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace launcher {
// Wakes the UI task on a button press or a touch-controller interrupt, so the
// runtime can wait for its next deadline instead of polling every 10ms while
// nothing is touched (docs/task8/plan.md 8-4). Ported from MuteHid's InputWake.
// False when the interrupts could not be installed; the caller must then keep
// polling, since no press would ever end a long wait.
bool beginInputWake(TaskHandle_t task);
// Interrupts are level-triggered and disable themselves on firing; call before
// each wait to re-enable the pins that are back at their idle (high) level.
void rearmInputWake();
}
