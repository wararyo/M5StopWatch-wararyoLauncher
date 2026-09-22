#pragma once
#include "ui/DisplayModel.h"
namespace launcher {
struct ScreenOutcome {
    bool changed=false,leave=false;
    const char* notice=nullptr; // Shown by ScreenManager as the usual toast.
};
// The contract from plan.md 4.1, extracted now that settings and the external
// app detail make it two implementations. The display model stays per-screen:
// ScreenManager composes it, so it is not part of this interface.
class AppScreen {
public:
    virtual ~AppScreen()=default;
    virtual void resize(int width,int height)=0;
    // Whether the entry may be opened at all. An unbound screen behaves like an
    // unimplemented one rather than opening empty.
    virtual bool available() const=0;
    // `now` is the monotonic time, so a screen that shows a running
    // measurement has its first frame right rather than one deadline late.
    virtual void enter(TimeUs now)=0;
    virtual void exit()=0;
    virtual ScreenOutcome handle(const Events& e,TimeUs now)=0;
    // Absolute deadline for the screen's own display update, INT64_MAX when it
    // is static. The runtime blocks until the earliest deadline it is given.
    virtual TimeUs nextUpdate() const { return INT64_MAX; }
    // Called once that deadline passes. It re-samples whatever the screen
    // shows and re-arms the deadline; true means the display changed. Without
    // it the deadline would only wake the loop, never redraw, because the
    // runtime's own display deadline is unset while an app screen covers the
    // clock (AppRuntime::step, `model.transition<1`).
    virtual bool tick(TimeUs now) { (void)now; return false; }
    // True while the screen is in a stretch that must not be interrupted, not
    // even by home. Only the boot commit of plan.md 8.2 uses it.
    virtual bool exclusive() const { return false; }
};
}
