#include "HostRuntime.h"
#include "host/FrameComposer.h"
#ifdef LAUNCHER_RENDER_METRICS
#include "host/RenderDiagnostics.h"
#endif
namespace launcher {
void HostRuntime::begin() { power_.begin(hal_.now()); nextInput_ = nextUsb_ = hal_.now(); renderer_.invalidate(); }
void HostRuntime::step() {
    const TimeUs now = hal_.now();
    const bool wasOff = power_.screenOff();
    // Asked every pass so a notification is consumed even when input is due
    // anyway. It only brings a read forward while nothing is being followed:
    // the touch driver skips its I2C read when asked again within 10ms of a
    // read that saw no touch and INT is already high again, so reads while
    // following stay one input period apart (docs/task8/plan.md 8-4).
    const bool pending = hal_.inputPending();
    if (pending) followUntil_ = std::max(followUntil_, now + FollowUs);
    if (now >= nextInput_ || (pending && nextInput_ == INT64_MAX)) {
        const auto raw = hal_.sampleInput();
        const auto e = input_.update(now, raw, wasOff && raw.touching);
        // Release edges also count as activity. Process home before screen events.
        power_.update(now, e.activity, screens_.active());
        if (e.home || e.next || e.decide || e.gesture != Gesture::None) {
            const bool changed = screens_.handle(e, now);
#ifdef LAUNCHER_RENDER_METRICS
            if (changed) recordInput(now);
#endif
            dirty_ = changed || dirty_;
        }
        // A held button or finger is followed at the input period: its
        // release, the 600ms home hold and drags need samples between
        // interrupts. So is the short stretch after an interrupt, because the
        // touch controller raises INT before its first report is readable.
        // Otherwise the next interrupt ends the wait (work 8-4), so an idle
        // loop wakes only for its deadlines.
        const bool follow = raw.a || raw.b || raw.touching || now < followUntil_;
        nextInput_ = follow ? now + 10000 : INT64_MAX;
    } else power_.update(now, false, screens_.active());
    // Cheap enough to do every pass. The worker notifies the UI task when it
    // publishes, so a result also ends a long idle wait.
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
#ifdef LAUNCHER_RENDER_METRICS
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
        const auto effective=screens_.effectiveSettings();
        if (effective.brightness != appliedBrightness_) {
            hal_.setBrightness(effective.brightness); appliedBrightness_ = effective.brightness;
        }
        power_.setTimeout(TimeUs(effective.screenOffSec) * 1000000);
        const auto watch = data_.sample(now);
        renderer_.draw(model, watch);
        // The clock's own deadlines only while the composition shows it: an
        // open screen or the raised list drives its frames by itself.
        nextDisplay_ = clockVisible(model) ?
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
    if (screens_.commitPendingBoot()) dirty_ = true;
}
void HostRuntime::wait() {
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
