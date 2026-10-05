#pragma once
#include "core/Time.h"
#include "host/ScreenId.h"
#include <cstdint>
namespace launcher {
// What a feature asks of the host to get the wearer's attention right now
// (docs/task12/plan.md 2.4). It is the whole of the current request, not a
// change: the host reflects it every step, so a source that stops asking
// lets the panel time out and the motor stop without telling anyone.
struct AttentionRequest {
    bool active=false;
    // Brought forward when the request starts, over whatever is shown.
    bool present=false;
    ScreenId screen=ScreenId::Home;
    // The panel stays lit until then, which then counts as the last input.
    // 0: no hold. Only a started request holds.
    TimeUs holdUntil=0;
    uint8_t vibration=0; // 0..255, while started.
};
// A feature's side of an attention request: a timer that ran out, say. The
// feature reads its own service and decides; the host owns the panel, the
// input, the motor and which screen is shown, and applies the same rules to
// every source.
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
    // The host has started the request at `now`: what the source times from
    // that moment (a vibration pattern) starts here, not at the expiry, so a
    // request held back by a boot commit still runs from its beginning.
    virtual void attended(TimeUs now)=0;
    // When to be asked again even with the panel dark and nothing pressed:
    // the next point where the request starts or changes. INT64_MAX: never.
    virtual TimeUs nextAttention() const=0;
};
}
