#pragma once
// Work 8-2: history of the USB bus activity, to test whether the host keeps the
// bus suspended until it opens the CDC port. Measurement build only.
#ifdef LAUNCHER_USB_DIAG
#include "power/PowerManager.h"
namespace launcher {
// `sof` is usb_serial_jtag_is_connected(): SOF packets arrive only while the
// host is not suspending the bus, so it reads 0 on a suspended cable too.
void usbDiag(TimeUs now, const PowerManager& power);
}
#endif
