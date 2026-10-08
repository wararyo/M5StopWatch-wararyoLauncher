#include "TestScreens.h"
#include "host/HomeGesture.h"
#include "host/FrameComposer.h"
#include "features/external/ExternalLayout.h"
#include "features/pedometer/PedometerLayout.h"
#include "features/settings/SettingsLayout.h"
#include "features/stopwatch/StopwatchLayout.h"
#include "features/timer/TimerLayout.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
using namespace launcher;
namespace {
const Viewport View{468,468};
Events touchEvent(Gesture g,int y,int x=234) { Events e{}; e.gesture=g; e.x=x; e.y=y; return e; }
Events chordEvent(bool held,TimeUs since) { Events e{}; e.chord=held; e.chordChanged=true; e.chordSince=since; return e; }
bool near(float a,float b,float within=0.6f) { return std::fabs(a-b)<=within; }
Rect rect(int x,int y,int w,int h) { return {x,y,w,h}; }
}
// docs/task14/plan.md 2.1, 2.2, 2.4, 2.5: the numbers themselves.
void layout() {
    CHECK(homeGestureEdge(View)==40 && homeGestureStarts(View,40) && !homeGestureStarts(View,41));
    CHECK(homeGestureCommitY(View)==120 && homeGestureCommits(View,120) && !homeGestureCommits(View,119));
    CHECK(homeBandMax(View)==80);
    // Stuck to the finger at the top, slower further down, never the maximum.
    CHECK(homeBandHeight(View,0)==0 && homeBandHeight(View,-5)==0);
    CHECK(near(homeBandHeight(View,1),1,0.02f));
    CHECK(near(homeBandHeight(View,20),17.7f) && near(homeBandHeight(View,80),50.6f) &&
          near(homeBandHeight(View,120),62.2f) && near(homeBandHeight(View,240),76.0f));
    float last=0;
    for (int y=1;y<=468;++y) { const float h=homeBandHeight(View,y); CHECK(h>last && h<80); last=h; }
    // Easings.
    CHECK(easeOutQuint(0)==0 && easeOutQuint(1)==1 && easeOutQuint(2)==1 && near(easeOutQuint(0.5f),0.96875f,1e-5f));
    CHECK(easeInOutCubic(0)==0 && easeInOutCubic(1)==1 && near(easeInOutCubic(0.5f),0.5f,1e-6f));
    CHECK(near(easeInOutCubic(0.25f),0.0625f,1e-6f) && near(easeInOutCubic(0.75f),0.9375f,1e-6f));
    // A+B: 0, half, full over the 600ms of the hold.
    CHECK(homeChordBand(View,0,600000)==0 && near(homeChordBand(View,300000,600000),40,1e-3f) &&
          homeChordBand(View,600000,600000)==80 && homeChordBand(View,900000,600000)==80);
    // Shrinking: from where it was let go to nothing in 100ms, quickly at first.
    CHECK(homeBandShrinking(60,0)==60 && homeBandShrinking(60,HomeBandShrinkUs)==0);
    CHECK(homeBandShrinking(60,16000)<40);
    // The clock coming in: from the band to the bottom in 800ms.
    CHECK(homeRevealEdge(View,80,0)==80 && homeRevealEdge(View,80,HomeRevealUs)==468);
    CHECK(homeRevealEdge(View,80,HomeRevealUs/2)>450);
    int edge=80;
    for (TimeUs t=HomeGestureFrameUs;t<=HomeRevealUs;t+=HomeGestureFrameUs) {
        const int next=homeRevealEdge(View,80,t);
        CHECK(next>=edge && next-edge<=40); // The largest step is the first.
        edge=next;
    }
}
// The pair's progress, as the input reports it.
void chordInput() {
    InputController c;
    auto e=c.update(0,{true});
    CHECK(!e.chord && !e.chordChanged);
    e=c.update(10000,{true,true});
    CHECK(e.chord && e.chordChanged && e.chordSince==10000);
    e=c.update(20000,{true,true});
    CHECK(e.chord && !e.chordChanged && e.chordSince==10000);
    e=c.update(610000,{true,true});
    CHECK(e.home && !e.chord && e.chordChanged); // Home ends it.
    e=c.update(620000,{true,true});
    CHECK(!e.chord && !e.chordChanged);
    e=c.update(630000,{});
    CHECK(!e.chord && !e.chordChanged);
    // Let go early.
    c.update(1000000,{true,true});
    e=c.update(1300000,{true});
    CHECK(!e.chord && e.chordChanged && !e.home);
    c.update(1400000,{});
    // Spent.
    c.update(2000000,{true,true});
    c.discardHeld();
    e=c.update(2010000,{true,true});
    CHECK(!e.chord && e.chordChanged);
    CHECK(!c.update(2700000,{true,true}).chord);
}
// The controller alone: who owns a touch and what the band does.
void swipe() {
    HomeGesture g(View);
    // Below the edge: not the system's.
    auto out=g.handle(touchEvent(Gesture::TouchStart,41),0,true,true);
    CHECK(!out.consumed && !out.changed);
    CHECK(!g.handle(touchEvent(Gesture::DragStart,80),10000,true,true).consumed);
    g.handle(touchEvent(Gesture::DragEnd,200),20000,true,true);
    CHECK(!g.active() && g.model().band==0);
    // At the edge, but no app screen: not the system's either.
    CHECK(!g.handle(touchEvent(Gesture::TouchStart,10),30000,false,true).consumed);
    g.handle(touchEvent(Gesture::Tap,10),40000,false,true);
    // At the edge of an app screen: every event of it is kept from the screen.
    out=g.handle(touchEvent(Gesture::TouchStart,10),100000,true,true);
    CHECK(out.consumed && !out.changed && g.model().band==0);
    out=g.handle(touchEvent(Gesture::DragStart,30,10),110000,true,true);
    CHECK(out.consumed && out.changed && g.model().band==int(std::lround(homeBandHeight(View,30))));
    // x plays no part.
    out=g.handle(touchEvent(Gesture::DragMove,80,400),120000,true,true);
    CHECK(out.consumed && g.model().band==51 && g.nextUpdate()==INT64_MAX);
    // Let go above the line: no home, the band shrinks in 100ms.
    out=g.handle(touchEvent(Gesture::DragEnd,119),130000,true,true);
    CHECK(out.consumed && out.changed && !out.home);
    CHECK(g.active() && g.nextUpdate()==130000+HomeGestureFrameUs);
    CHECK(!g.update(140000));
    CHECK(g.update(146000) && g.model().band<62 && g.model().band>0);
    CHECK(g.update(230000) && g.model().band==0 && !g.active() && g.nextUpdate()==INT64_MAX);
    // A tap at the edge: kept, and nothing at all happens.
    g.handle(touchEvent(Gesture::TouchStart,5),300000,true,true);
    out=g.handle(touchEvent(Gesture::Tap,5),310000,true,true);
    CHECK(out.consumed && !out.changed && !g.active());
    // Let go at the line: home.
    g.handle(touchEvent(Gesture::TouchStart,20),400000,true,true);
    g.handle(touchEvent(Gesture::DragStart,40),410000,true,true);
    out=g.handle(touchEvent(Gesture::DragEnd,120),420000,true,true);
    CHECK(out.consumed && out.home);
    g.reset();
    CHECK(!g.active() && g.model().band==0);
    // A new swipe takes up a band still shrinking.
    g.handle(touchEvent(Gesture::TouchStart,20),500000,true,true);
    g.handle(touchEvent(Gesture::DragStart,100),510000,true,true);
    g.handle(touchEvent(Gesture::DragEnd,100),520000,true,true);
    g.update(536000);
    g.handle(touchEvent(Gesture::TouchStart,20),540000,true,true);
    CHECK(g.active());
    g.handle(touchEvent(Gesture::DragStart,200),550000,true,true);
    CHECK(g.model().band==int(std::lround(homeBandHeight(View,200))) && g.nextUpdate()==INT64_MAX);
    // Reset mid-swipe (an attention request): the band and the touch go.
    g.reset();
    CHECK(!g.active() && g.model().band==0);
    CHECK(!g.handle(touchEvent(Gesture::DragMove,250),560000,true,true).consumed);
}
void chord() {
    HomeGesture g(View);
    // Nothing to close: no band.
    CHECK(!g.handle(chordEvent(true,0),0,false,false).changed && !g.active());
    g.handle(chordEvent(false,0),100000,false,false);
    // Grows with the hold, at a frame rate, until it is full.
    auto out=g.handle(chordEvent(true,1000000),1000000,false,true);
    CHECK(out.changed && g.active() && g.model().band==0 && g.nextUpdate()==1016000);
    CHECK(g.update(1300000) && g.model().band==40);
    CHECK(g.update(1600000) && g.model().band==80 && g.nextUpdate()==INT64_MAX);
    g.reset(); // Home: the manager leaves everything, and the band goes.
    // Let go early: the band shrinks from where it got to.
    g.handle(chordEvent(true,2000000),2000000,false,true);
    g.update(2300000);
    out=g.handle(chordEvent(false,2000000),2310000,false,true);
    CHECK(out.changed && g.model().band==40 && g.nextUpdate()==2326000);
    g.update(2410000);
    CHECK(!g.active() && g.model().band==0);
    // The hold takes over a swipe; that touch stays the system's and moves nothing.
    g.handle(touchEvent(Gesture::TouchStart,20),3000000,true,true);
    g.handle(touchEvent(Gesture::DragStart,100),3010000,true,true);
    g.handle(chordEvent(true,3020000),3020000,true,true);
    CHECK(g.model().band==0);
    out=g.handle(touchEvent(Gesture::DragMove,300),3030000,true,true);
    CHECK(out.consumed && !out.changed && g.model().band==0);
    out=g.handle(touchEvent(Gesture::DragEnd,300),3040000,true,true);
    CHECK(out.consumed && !out.home);
    CHECK(g.update(3320000) && g.model().band==40);
    // Nor does a touch that lands at the edge while the hold shows.
    g.handle(touchEvent(Gesture::TouchStart,10),3330000,true,true);
    out=g.handle(touchEvent(Gesture::DragStart,200),3340000,true,true);
    CHECK(out.consumed && !out.changed);
}
// Through ScreenManager: ahead of the screens, and home as A+B leaves it.
void manager() {
    TestScreens s;
    CHECK(s.present(ScreenId::Timer,0) && s.model().screen==ScreenId::Timer);
    // A short swipe: the band, then nothing.
    s.handle(touchEvent(Gesture::TouchStart,20),10000);
    CHECK(s.handle(touchEvent(Gesture::DragStart,90),20000) && s.model().homeGesture.band>0 && s.active());
    s.handle(touchEvent(Gesture::DragEnd,90),30000);
    CHECK(s.model().screen==ScreenId::Timer && s.nextUpdate()<=46000);
    s.update(200000);
    CHECK(s.model().homeGesture.band==0 && !s.active());
    // Far enough: home, counted as A+B counts it, with the timer left.
    s.handle(touchEvent(Gesture::TouchStart,20),300000);
    s.handle(touchEvent(Gesture::DragStart,90),310000);
    s.handle(touchEvent(Gesture::DragMove,200),320000);
    CHECK(s.handle(touchEvent(Gesture::DragEnd,200),330000));
    CHECK(s.model().screen==ScreenId::Home && s.model().homeCount==1 && s.model().homeGesture.band==0);
    // The clock comes in over what the timer left, and then it is home.
    CHECK(s.revealing() && s.model().homeGesture.revealing && s.active() && !s.homeAtRest());
    s.update(330000+HomeRevealUs);
    CHECK(!s.revealing() && !s.model().homeGesture.revealing && !s.active() && s.homeAtRest());
    // On the clock and the list the edge is theirs: the list's own pull home.
    Events next{}; next.next=true;
    s.handle(next,400000); s.update(600000);
    CHECK(s.model().screen==ScreenId::AppList && s.model().launcher.transition==1);
    auto pull=[](Gesture g,int y) { auto e=touchEvent(g,y); e.totalY=y-20; return e; };
    s.handle(pull(Gesture::TouchStart,20),700000);
    s.handle(pull(Gesture::DragStart,60),710000);
    CHECK(s.model().homeGesture.band==0 && s.model().launcher.transition<1);
    s.handle(pull(Gesture::DragEnd,200),720000);
    s.update(1000000);
    CHECK(s.model().screen==ScreenId::Home && s.model().homeCount==1);
    // A+B on the clock shows nothing; on an app screen it does, and an
    // attention request that brings a screen forward takes it away at once.
    s.handle(chordEvent(true,1100000),1100000);
    CHECK(s.model().homeGesture.band==0 && !s.active());
    s.handle(chordEvent(false,1100000),1200000);
    s.present(ScreenId::Timer,1300000);
    s.handle(chordEvent(true,1400000),1400000);
    s.update(1700000);
    CHECK(s.model().homeGesture.band==40);
    CHECK(s.present(ScreenId::Timer,1710000) && s.model().homeGesture.band==0 && !s.active());
    // A+B home closes it the usual way, band and all.
    s.handle(chordEvent(true,1800000),1800000);
    s.update(2100000);
    Events home{}; home.home=true; home.chordChanged=true;
    CHECK(s.handle(home,2400000));
    CHECK(s.model().screen==ScreenId::Home && s.model().homeCount==2 && s.model().homeGesture.band==0);
    CHECK(s.model().homeGesture.revealing);
}
// The clock coming in after home (docs/task14/plan.md 2.5).
void reveal() {
    HomeGesture g(View);
    g.reveal(70,1000000);
    auto m=g.model();
    CHECK(g.revealing() && g.active() && m.revealing && m.band==0 && m.edge==70);
    CHECK(g.nextUpdate()==1000000+HomeGestureFrameUs);
    int edge=70;
    TimeUs now=1000000;
    while (g.revealing()) {
        now+=HomeGestureFrameUs;
        CHECK(g.update(now));
        if (!g.revealing()) break;
        m=g.model();
        CHECK(m.edge>=edge && m.edge<=468 && g.nextUpdate()==now+HomeGestureFrameUs);
        edge=m.edge;
    }
    // Over at 800ms, no sooner, and then nothing more to draw.
    CHECK(now-1000000>=HomeRevealUs && now-1000000<HomeRevealUs+HomeGestureFrameUs);
    CHECK(!g.active() && !g.model().revealing && g.nextUpdate()==INT64_MAX && !g.update(now+100000));
    // An attention request ends it at once.
    g.reveal(80,0); g.update(100000); g.reset();
    CHECK(!g.revealing() && !g.model().revealing);
}
// Turning the edge into what each frame draws: everything up to the edge on
// the first frame, then the rows the edge moved over, then the rest.
void revealFrames() {
    HomeRevealTracker t;
    HomeGestureModel g;
    auto f=t.next(g,View);
    CHECK(!f.start && f.confine.empty() && f.strip.empty());
    g.revealing=true; g.edge=70;
    f=t.next(g,View);
    CHECK(f.start && f.confine==rect(0,0,468,70) && f.strip.empty());
    g.edge=110; f=t.next(g,View);
    CHECK(!f.start && f.confine==rect(0,0,468,110) && f.strip==rect(0,70,468,40));
    f=t.next(g,View); // The edge stood still: nothing new.
    CHECK(f.confine==rect(0,0,468,110) && f.strip.empty());
    g.edge=468; f=t.next(g,View);
    CHECK(f.strip==rect(0,110,468,358) && f.confine==rect(0,0,468,468));
    g.revealing=false; f=t.next(g,View);
    CHECK(!f.start && f.confine.empty() && f.strip.empty());
    // Over before the edge reached the bottom: the rest, unconfined.
    g.revealing=true; g.edge=0; f=t.next(g,View);
    CHECK(f.start && f.confine==rect(0,0,468,1));
    g.edge=300; t.next(g,View);
    g.revealing=false; f=t.next(g,View);
    CHECK(f.confine.empty() && f.strip==rect(0,300,468,168));
    CHECK(t.next(g,View).strip.empty());
    // Coming in is no full repaint; anything else that changes the screen is.
    FrameComposer c;
    FrameModel m; m.viewport=View; m.screen=ScreenId::Timer;
    CHECK(c.compose(m).changed);
    m.screen=ScreenId::Home; m.homeGesture.revealing=true; m.homeGesture.edge=80;
    CHECK(!c.compose(m).changed);
    m.homeGesture={};
    CHECK(!c.compose(m).changed);
    m.screen=ScreenId::Timer; CHECK(c.compose(m).changed);
}
// Nothing gets through while the clock comes in, and home comes in only
// over something.
void revealInput() {
    TestScreens s;
    s.present(ScreenId::Timer,0);
    s.handle(touchEvent(Gesture::TouchStart,20),10000);
    s.handle(touchEvent(Gesture::DragStart,200),20000);
    s.handle(touchEvent(Gesture::DragEnd,200),30000);
    CHECK(s.revealing());
    CHECK(s.model().homeGesture.edge==int(std::lround(homeBandHeight(View,200))));
    // A, B, a tap on the clock's APPS, A+B: none of it lands.
    Events next{}; next.next=true;
    s.handle(next,100000);
    Events tap=touchEvent(Gesture::Tap,380); tap.touchUs=50000;
    s.handle(touchEvent(Gesture::TouchStart,380),200000); s.handle(tap,250000);
    Events home{}; home.home=true;
    s.handle(home,300000);
    CHECK(s.model().screen==ScreenId::Home && s.home.events==0 && s.model().homeCount==1 && s.revealing());
    s.update(30000+HomeRevealUs);
    CHECK(!s.revealing());
    s.handle(touchEvent(Gesture::TouchStart,380),900000); s.handle(tap,950000);
    CHECK(s.home.events==1 && s.model().screen==ScreenId::AppList); // APPS, now the clock's.
    // A+B over the list: the clock comes in from the full band.
    s.update(2000000);
    s.handle(chordEvent(true,2000000),2000000);
    s.update(2600000);
    home=Events{}; home.home=true; home.chordChanged=true;
    s.handle(home,2600000);
    CHECK(s.revealing() && s.model().homeGesture.edge==80);
    // An attention request while it comes in: the timer, as usual.
    CHECK(s.present(ScreenId::Timer,2700000) && !s.revealing() && s.model().screen==ScreenId::Timer);
    // A+B on the clock itself: nothing to come in over.
    home=Events{}; home.home=true;
    s.handle(home,2800000);
    s.update(2800000+HomeRevealUs);
    s.handle(home,3700000);
    CHECK(!s.revealing() && s.model().homeCount==4);
}
// A touch that lands at the top edge is the system's, so no screen puts a
// target there (docs/plan.md 5.1). The menus' rows scroll through it and are
// the one allowed exception: a part of a row there takes no tap.
void topEdgeHasNoTargets() {
    const int edge=homeGestureEdge(View);
    auto clear=[&](auto hits) {
        for (int y=0;y<=edge;++y) for (int x=0;x<View.width;++x) if (hits(x,y)) return false;
        return true;
    };
    for (auto view:{TimerView::Setup,TimerView::Countdown,TimerView::Ringing})
        CHECK(clear([&](int x,int y) { return hitTimer(View,view,x,y).kind!=TimerHit::None; }));
    CHECK(clear([&](int x,int y) { return hitStopwatch(View,x,y).kind!=StopwatchHit::None; }));
    CHECK(clear([&](int x,int y) { return pedometerOkHitBox(View).contains(x,y); }));
    for (auto phase:{ExternalPhase::Browsing,ExternalPhase::BootCommitting,ExternalPhase::BootFailed}) {
        ExternalModel external; external.phase=phase;
        CHECK(clear([&](int x,int y) { return hitExternal(View,external,x,y).kind!=ExternalHit::None; }));
    }
    for (auto view:{SettingsView::DateTime,SettingsView::Brightness,SettingsView::ScreenOff,SettingsView::Info}) {
        SettingsGeometry geometry; static_cast<Viewport&>(geometry)=View; geometry.view=view;
        CHECK(clear([&](int x,int y) { return hitSettings(geometry,x,y).kind!=SettingsHit::None; }));
    }
}
int main() {
    layout(); chordInput(); swipe(); chord(); manager(); topEdgeHasNoTargets(); reveal(); revealFrames(); revealInput();
    std::cout << "PASS: home gesture layout, chord input, swipe, chord band, screen manager, top edge free of targets, "
                 "reveal, reveal frames, input while revealing\n";
}
