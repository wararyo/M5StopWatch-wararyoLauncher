#include "AppRuntime.h"
namespace launcher {
void AppRuntime::begin() { power_.begin(hal_.now()); nextInput_ = nextUsb_ = hal_.now(); }
void AppRuntime::step() {
    const TimeUs now = hal_.now();
    const bool wasOff = power_.screenOff();
    if (now >= nextInput_) {
        const auto raw = hal_.sampleInput();
        const auto e = input_.update(now, raw, wasOff && raw.touching);
        // Release edges also count as activity. Process home before screen events.
        power_.update(now, e.activity, screens_.active());
        dirty_ = screens_.handle(e) || dirty_;
        nextInput_ = now + 10000;
    } else power_.update(now, false, screens_.active());
    if (now >= nextUsb_) {
        const auto usb = hal_.sampleUsb();
        if (usb.vbusValid != power_.usb.vbusValid || usb.powered() != power_.usb.powered() ||
            usb.dataConnected != power_.usb.dataConnected) dirty_ = true;
        power_.usb = usb;
        nextUsb_ = now + 1000000;
    }
    if (wasOff != power_.screenOff()) { hal_.setScreenOff(power_.screenOff()); dirty_ = true; }
    if (!power_.screenOff() && (dirty_ || now >= screens_.nextUpdate())) {
        hal_.draw(screens_.model(), power_.usb);
        dirty_ = false;
    }
}
void AppRuntime::wait() {
    const auto now = hal_.now();
    // Keep overdue work due until step() actually services it. vTaskDelay can
    // wake before an absolute deadline (tick phase); rebasing here would then
    // postpone unsampled input forever under continuous processing overruns.
    // step() rebases each serviced period to now, without replaying missed work.
    // waitDelay still guarantees at least one blocking tick when work is due.
    auto deadline = std::min(nextInput_, std::min(nextUsb_, power_.deadline()));
    if (!power_.screenOff()) deadline = std::min(deadline, screens_.nextUpdate());
    hal_.waitUs(waitDelay(now, deadline));
}
}
