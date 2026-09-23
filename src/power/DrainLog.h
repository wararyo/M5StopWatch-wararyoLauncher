#pragma once
// Battery-voltage record for runs on battery (docs/task8/plan.md 8-0). Only the
// m5stopwatch-drain build carries it; the product build has no instrument.
#ifdef LAUNCHER_DRAIN_LOG
#include "power/PowerManager.h"
namespace launcher {
// NVS must be initialized first. Switches stdin to non-blocking for `O`.
void beginDrainLog();
// Call from the UI task: it reads the PMIC over the shared I2C bus. Recording
// starts by itself when USB power goes away, so starting a run never needs the
// serial port, whose opening adds 16mA until the next reset.
void drainLog(TimeUs now, const PowerManager& power);
}
#endif
