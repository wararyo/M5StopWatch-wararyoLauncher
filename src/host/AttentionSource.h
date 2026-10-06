#pragma once
#include "core/Time.h"
#include "host/ScreenId.h"
namespace launcher {
// What a feature asks of the host to get the wearer's attention right now
// (docs/task12/plan.md 2.4): whether it does, and which screen to bring
// forward. It is the whole of the current request, not a change; the host
// acts once when a request starts. How the screen then alerts (the motor, how
// long the panel stays lit) is the presented screen's own (host/Screen.h).
struct AttentionRequest {
    bool active=false;
    // Brought forward when the request starts, over whatever is shown.
    bool present=false;
    ScreenId screen=ScreenId::Home;
};
// A feature's side of an attention request: a timer that ran out, say. The
// feature reads its own service and decides; the host owns the panel, the
// input and which screen is shown, and applies the same rules to every source.
//
// When a request starts the host spends whatever is pressed (its release, a
// repeat, a long press or an A+B hold), lights a dark panel and presents the
// screen, all before the step's input, so a press that arrives with the
// expiry reaches nothing. During a boot commit a request waits.
class AttentionSource {
public:
    virtual ~AttentionSource()=default;
    // Asked at the start of every step, before input. It may settle its
    // service here (a timer that reached its end rings).
    virtual AttentionRequest attention(TimeUs now)=0;
    // When a request may start, to be asked then even with the panel dark and
    // nothing pressed. INT64_MAX: nothing pending.
    virtual TimeUs nextAttention() const=0;
};
}
