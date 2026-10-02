#pragma once
// Charging record for the charge curve. Only the m5stopwatch-charge build
// carries it, on top of the drain record, so one flash covers a discharge to
// empty and the charge that follows.
#ifdef LAUNCHER_CHARGE_LOG
#include "power/PowerManager.h"
namespace launcher {
// Re-enables charging, which a pause left off if the chip restarted inside
// one: the PMIC keeps its registers across a restart and so across flashing.
void beginChargeLog();
// Call from the UI task after the runtime step, which refreshes power.usb. On
// USB power it charges for 10 minutes, then stops charging for 3 so the battery
// rests and its voltage can be read against the discharge curve.
// USB power back within 5 minutes continues the same record.
void chargeLog(TimeUs now, const PowerManager& power);
// `C` on the serial port; DrainLog owns stdin and hands the byte over.
void dumpChargeLog();
}
#endif
