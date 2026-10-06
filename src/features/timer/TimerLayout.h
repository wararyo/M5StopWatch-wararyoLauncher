#pragma once
#include "ui/rendering/Scale.h"
#include "ui/rendering/Geometry.h"
#include "features/timer/TimerModel.h"
namespace launcher {
// The timer's three views, laid out from docs/Images/Timer (Setup.svg is the
// measured one, 466 across). Coordinates are taken about the centre of that
// drawing and scaled like every other screen, so the circle's margins hold on
// the 468 panel. Drawing and hit testing share these boxes.
inline int timerX(const Viewport& m,int x) { return m.width/2+offsetPx(m,x-233); }
inline int timerY(const Viewport& m,int y) { return m.height/2+offsetPx(m,y-233); }
inline Rect timerBox(const Viewport& m,int x,int y,int w,int h) {
    const int left=timerX(m,x),top=timerY(m,y);
    return {left,top,timerX(m,x+w)-left,timerY(m,y+h)-top};
}
inline int timerCorner(const Viewport& m) { return offsetPx(m,8); }

// Setup: the hourglass, HH : MM : SS, keys 1-9 with a tall 0, and SET.
inline Rect timerSetupIconBox(const Viewport& m) { return timerBox(m,211,17,44,44); }
inline Rect timerFieldBox(const Viewport& m,int field) { return timerBox(m,82+107*field,78,89,64); }
inline Rect timerColonBox(const Viewport& m,int index) { return timerBox(m,171+107*index,78,18,64); }
// Digit 1..9 in rows of three; 0 is the tall key on the right.
inline Rect timerKeyBox(const Viewport& m,int digit) {
    if (digit==0) return timerBox(m,367,173,48,184);
    const int column=(digit-1)%3,row=(digit-1)/3;
    return timerBox(m,55+104*column,173+64*row,96,56);
}
// Where a finger lands on a key. The tall 0 is narrow and sits near the bezel,
// so it takes touches from as far again to its right, up to the glass edge;
// the others take what they show.
inline Rect timerKeyHitBox(const Viewport& m,int digit) {
    return digit==0 ? timerBox(m,367,173,96,184) : timerKeyBox(m,digit);
}
inline Rect timerSetBox(const Viewport& m) { return timerBox(m,153,381,160,44); }
// SET is low and thin above the bezel, so it takes touches from as far again
// below it as it is tall; what it shows stays the same.
inline Rect timerSetHitBox(const Viewport& m) { return timerBox(m,153,381,160,88); }

// Countdown: the hourglass, RESET and PAUSE, then the time.
inline Rect timerCountdownButtonBox(const Viewport& m,int index) { return timerBox(m,index==0 ? 82 : 243,119,141,70); }
inline Rect timerCountdownTimeBox(const Viewport& m) { return timerBox(m,68,228,330,68); }

// Ringing: the hourglass lower down, the time since, and the button.
inline Rect timerRingingIconBox(const Viewport& m) { return timerBox(m,211,101,44,44); }
inline Rect timerRingingTimeBox(const Viewport& m) { return timerBox(m,68,170,330,68); }
inline Rect timerDismissBox(const Viewport& m) { return timerBox(m,133,280,200,70); }

struct TimerHit {
    enum Kind { None,Field,Key,Set,Reset,Pause,Dismiss } kind=None;
    int index=0;
};
inline TimerHit hitTimer(const Viewport& m,TimerView view,int x,int y) {
    switch (view) {
    case TimerView::Setup:
        for (int i=0;i<TimerFieldCount;++i) if (timerFieldBox(m,i).contains(x,y)) return {TimerHit::Field,i};
        for (int d=0;d<10;++d) if (timerKeyHitBox(m,d).contains(x,y)) return {TimerHit::Key,d};
        if (timerSetHitBox(m).contains(x,y)) return {TimerHit::Set};
        break;
    case TimerView::Countdown:
        if (timerCountdownButtonBox(m,0).contains(x,y)) return {TimerHit::Reset};
        if (timerCountdownButtonBox(m,1).contains(x,y)) return {TimerHit::Pause};
        break;
    case TimerView::Ringing:
        if (timerDismissBox(m).contains(x,y)) return {TimerHit::Dismiss};
        break;
    }
    return {};
}
// B on a field: one up, 59 back to 0 for minutes and seconds, 99 back to 0
// for anything (docs/task12/plan.md 2.2). A typed 75 keeps counting to 99:
// carrying is left to the start.
inline int stepTimerField(int field,int value) {
    if (field>0 && value==59) return 0;
    return value>=99 ? 0 : value+1;
}
// A key on a field: the digit enters from the right, the older one moving left.
inline int typeTimerField(int value,int digit) { return (value%10)*10+digit; }
// The length as fields, for a setup that starts from it.
inline void splitTimer(int32_t seconds,int fields[TimerFieldCount]) {
    fields[0]=int(seconds/3600); fields[1]=int(seconds/60%60); fields[2]=int(seconds%60);
}
}
