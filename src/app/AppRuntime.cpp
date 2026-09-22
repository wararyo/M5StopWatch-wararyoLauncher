#include "AppRuntime.h"
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
#include "ui/RenderDiagnostics.h"
#endif
namespace launcher {
void AppRuntime::begin() { power_.begin(hal_.now()); nextInput_ = nextUsb_ = hal_.now(); renderer_.invalidate(); }
void AppRuntime::step() {
    const TimeUs now = hal_.now();
    const bool wasOff = power_.screenOff();
    if (now >= nextInput_) {
        const auto raw = hal_.sampleInput();
        const auto e = input_.update(now, raw, wasOff && raw.touching);
        // Release edges also count as activity. Process home before screen events.
        power_.update(now, e.activity, screens_.active());
        if (e.home || e.next || e.decide || e.gesture != Gesture::None) {
            const bool changed = screens_.handle(e, now);
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
            if (changed) recordInput(now);
#endif
            dirty_ = changed || dirty_;
        }
        nextInput_ = now + 10000;
    } else power_.update(now, false, screens_.active());
    // Cheap enough to do every pass: the loop already wakes at the input
    // period, so results reach the list without any extra wakeup.
    if (slots_) {
        SlotCatalog catalog;
        if (slots_->poll(catalog)) { screens_.setSlots(catalog); dirty_ = true; }
    }
    if (now >= nextUsb_) {
        const auto usb = hal_.sampleUsb();
        power_.usb = usb;
        nextUsb_ = now + 1000000;
    }
    if (wasOff != power_.screenOff()) {
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
        if (wasOff) recordWake(now);
#endif
        // The panel comes back at zero brightness, so the level is re-applied
        // on the first frame after the wake rather than inside the HAL.
        hal_.setScreenOff(power_.screenOff()); dirty_ = true; renderer_.invalidate();
        appliedBrightness_ = -1;
    }
    if (!power_.screenOff()) {
        dirty_ = screens_.update(now) || dirty_;
    }
    if (!power_.screenOff() && (dirty_ || now >= nextDisplay_)) {
        const auto model = screens_.model();
        // Both come from the display model, so a preview, a cancel and a home
        // discard all travel the same single path down to the HAL.
        if (model.brightness != appliedBrightness_) {
            hal_.setBrightness(model.brightness); appliedBrightness_ = model.brightness;
        }
        power_.setTimeout(TimeUs(model.screenOffSec) * 1000000);
        const auto watch = data_.sample(now);
        renderer_.draw(model, watch);
        nextDisplay_ = model.transition < 1 ?
            std::min(renderer_.nextUpdate(now, watch), data_.nextUpdate(now)) : INT64_MAX;
        // A misbehaving display provider must not make an overdue busy loop.
        if (nextDisplay_ <= now) nextDisplay_ = now + 16000;
        dirty_ = false;
        // The clock is on screen before the ~1s of flash reads begin, as
        // plan.md 8.1 asks.
        if (slots_ && !scanRequested_) { scanRequested_ = true; slots_->requestScan(); }
    }
    // After the draw above, so the "starting" frame reaches the panel before the
    // call blocks for the pre-boot re-verification and restarts. Entering the
    // phase marks the frame dirty, so that draw happens in this same step.
    // Deliberately outside the draw branch: a screen that went off must not
    // strand the commit, because the screen itself takes no input until it ends.
    if (screens_.commitPendingBoot(now)) dirty_ = true;
}
void AppRuntime::wait() {
    const auto now = hal_.now();
    // Keep overdue work due until step() actually services it. vTaskDelay can
    // wake before an absolute deadline (tick phase); rebasing here would then
    // postpone unsampled input forever under continuous processing overruns.
    // step() rebases each serviced period to now, without replaying missed work.
    // waitDelay still guarantees at least one blocking tick when work is due.
    auto deadline = std::min(nextInput_, std::min(nextUsb_, power_.deadline()));
    if (!power_.screenOff()) deadline = std::min(deadline, std::min(screens_.nextUpdate(), nextDisplay_));
    hal_.waitUs(waitDelay(now, deadline));
}
}
