#include "UsbDiag.h"
#ifdef LAUNCHER_USB_DIAG
#include <cstdio>
namespace launcher {
namespace {
// Only changes are kept, so a long unattended stretch costs one entry.
struct Event { uint32_t seconds; bool sof, vbus; };
constexpr int Capacity = 32;
Event events[Capacity];
int head = 0, count = 0;
bool known = false, lastSof = false, lastVbus = false;
// Whatever happened before the port opened cannot be printed then: nobody was
// reading. Wait until the host has had time to open it, then replay.
constexpr TimeUs ReplayDelayUs = 2000000;
TimeUs replayAt = -1;

void replay(TimeUs now) {
    std::printf("[UsbDiag] history now=%lus n=%d\n", (unsigned long)(now / 1000000), count);
    const int first = (head - count + Capacity) % Capacity;
    for (int i = 0; i < count; ++i) {
        const auto& e = events[(first + i) % Capacity];
        std::printf("[UsbDiag] t=%lus sof=%d vbus=%d\n", (unsigned long)e.seconds, int(e.sof), int(e.vbus));
    }
    std::fflush(stdout);
}
}
void usbDiag(TimeUs now, const PowerManager& power) {
    const bool sof = power.usb.dataConnected, vbus = power.usb.powered();
    if (!known || sof != lastSof || vbus != lastVbus) {
        events[head] = {uint32_t(now / 1000000), sof, vbus};
        head = (head + 1) % Capacity;
        if (count < Capacity) ++count;
        if (sof && (!known || !lastSof)) replayAt = now + ReplayDelayUs;
        known = true; lastSof = sof; lastVbus = vbus;
    }
    if (replayAt >= 0 && now >= replayAt) { replayAt = -1; replay(now); }
}
}
#endif
