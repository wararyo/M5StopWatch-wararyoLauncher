#include "app/Application.h"
#include "TestScreens.h"
#include "app/FrameComposer.h"
#include "features/launcher/AppListLayout.h"
#include "ui/rendering/Element.h"
#include "ui/list/ListController.h"
#include "ui/list/ListLayout.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <random>
#define CHECK(x) do { if(!(x)) { std::cerr<<__LINE__<<": " #x "\n"; std::exit(1); } } while(false)
using namespace launcher;
// The placement input and the renderer both use for this frame of the launcher.
ListPlacement placementOf(const FrameModel& m) {
    return appListPlacement(m.viewport,m.launcher.transition,m.launcher.list.scroll);
}
void navigation() {
    TestScreens s; TimeUs now=0; Events e{};
    e.gesture=Gesture::Tap; e.x=234; e.y=234;
    CHECK(!s.handle(e,now) && s.model().screen==ScreenId::Home);
    auto drag=[&](int dy) {
        e={}; e.gesture=Gesture::DragStart; e.totalY=dy; s.handle(e,now);
        e.gesture=Gesture::DragEnd; s.handle(e,now);
        now+=180000; s.update(now);
    };
    drag(-50); CHECK(s.model().screen==ScreenId::Home && s.model().launcher.transition==0);
    CHECK((composeFrame(s.model()).home.offsetY==0 &&
           composeFrame(s.model()).home.clip==Rect{0,0,468,468}));
    drag(-51); CHECK(s.model().screen==ScreenId::AppList && s.model().launcher.transition==1);
    CHECK(composeFrame(s.model()).home.offsetY==-468 && composeFrame(s.model()).home.clip.empty());
    for(int i=1;i<=5;++i) {
        e={}; e.next=true; s.handle(e,now); now+=180000; s.update(now);
        CHECK(s.model().launcher.list.selection==i%5);
        const auto row=layoutListRow(placementOf(s.model()),i%5);
        CHECK(row.box.contains(row.labelX,row.centerY));
    }
    drag(50); CHECK(s.model().screen==ScreenId::AppList);
    drag(51); CHECK(s.model().screen==ScreenId::Home);
    e={}; e.next=true; s.handle(e,now); now+=180000; s.update(now);
    drag(-200); CHECK(s.model().launcher.list.scroll>0);
    // Hitting the top during a list gesture does not turn it into Home.
    drag(1000); CHECK(s.model().launcher.list.scroll==0 && s.model().screen==ScreenId::AppList);
    drag(100); CHECK(s.model().screen==ScreenId::Home);
    e={}; e.next=true; s.handle(e,now); now+=180000; s.update(now);
    e={}; e.gesture=Gesture::Tap; e.x=0; e.y=0;
    CHECK(!s.handle(e,now) && !s.model().toast);
    const auto row=layoutListRow(placementOf(s.model()),1);
    e.x=row.labelX; e.y=row.centerY; s.handle(e,now);
    CHECK(s.model().launcher.list.selection==1 && s.model().toast);
    CHECK(s.nextUpdate()==now+1400000);
    now+=1400000; s.update(now); CHECK(!s.model().toast && s.nextUpdate()==INT64_MAX);
    e={}; e.gesture=Gesture::DragStart; e.totalX=100; e.totalY=10;
    s.handle(e,now); CHECK(!s.active());
    e.gesture=Gesture::DragEnd; s.handle(e,now); CHECK(!s.model().toast);
    e={}; e.next=true; s.handle(e,now);
    e={}; e.home=true; s.handle(e,now+10000);
    now+=10000000; CHECK(!s.update(now));
    CHECK(s.model().screen==ScreenId::Home && s.nextUpdate()==INT64_MAX);
    // Timing is elapsed-time based, not dependent on the number of frames.
    TestScreens a,b; e={}; e.next=true;
    a.handle(e,0); b.handle(e,0);
    for(int t=16000;t<=160000;t+=16000) a.update(t);
    b.update(160000); CHECK(a.model().launcher.transition==b.model().launcher.transition);
    b.update(10000000); CHECK(b.model().launcher.transition==1 && b.nextUpdate()==INT64_MAX);
    TestScreens rapid; e={}; e.next=true;
    rapid.handle(e,0); rapid.handle(e,10000); rapid.handle(e,20000);
    CHECK(rapid.model().launcher.list.selection==2);
    rapid.update(200000); CHECK(rapid.model().launcher.transition==1 && rapid.model().launcher.list.scroll==2*rowSpacing(rapid.model().viewport));
    for(int side: {400,466,468}) {
        const Viewport m{side,side};
        int previousX=side,previousDy=-side,highest=side,lowest=0;
        for(int scroll=0;scroll<=4*rowSpacing(m);scroll+=3) {
            const auto p=appListPlacement(m,1,float(scroll));
            for(int i=0;i<5;++i) {
                const auto r=layoutListRow(p,i); const auto box=r.box;
                if(box.empty()) continue;
                CHECK(box.x>=0 && box.y>=0 && box.x+box.w<=side && box.y+box.h<=side);
                // The box reaches the right edge so it covers a name the bezel
                // cuts off; only the icon has to stay on the panel.
                CHECK(box.x+box.w==side && box.x<=r.iconX-r.radius);
                CHECK(r.iconX-r.radius>=0 && r.iconX+r.radius<=side);
                CHECK(hitListRow(p,5,box.x+box.w/2,box.y+box.h/2)==i);
                const int dy=r.centerY-side/2;
                // A vertically centred row stops `listMargin` clear of the
                // bezel; every other row swings right of it, without steps.
                CHECK(r.iconX>=iconHomeX(m));
                CHECK(iconHomeX(m)-r.radius==listMargin(m));
                if(std::abs(dy)<std::abs(previousDy)) CHECK(r.iconX<=previousX);
                previousX=r.iconX; previousDy=dy;
                highest=std::min(highest,box.y); lowest=std::max(lowest,box.y+box.h);
                // The mask is pushed as a rectangle, so it has to stay inside
                // the smaller unselected circle and clear of the name.
                const int mask=scaled(m,IconMaskPx),small=r.radius-selectionGrowth(m);
                const auto icon=iconBox(r,mask,mask);
                CHECK(mask*mask/2<=small*small);
                CHECK(icon.x>=box.x && icon.x+icon.w<=r.labelX);
                CHECK(icon.y>=r.centerY-r.radius && icon.y+icon.h<=r.centerY+r.radius);
            }
        }
        // Rows reach both edges of the panel: no horizontal clip of their own
        // leaves a black strip that the round bezel would not have hidden.
        CHECK(highest==0 && lowest==side);
    }
}
void flick() {
    auto release=[](ScreenManager& s,float speed,int dy=-60) {
        Events e{}; e.next=true; s.handle(e,0); s.update(180000);
        e={}; e.gesture=Gesture::DragStart; e.totalY=dy; s.handle(e,200000);
        e.gesture=Gesture::DragEnd; e.velocityY=-speed; s.handle(e,250000);
    };
    TestScreens slow,fast,sparse;
    release(slow,0); release(fast,1000); release(sparse,1000);
    float previous=fast.model().launcher.list.scroll;
    for(int t=266000;t<430000;t+=16000) {
        fast.update(t);
        CHECK(fast.model().launcher.list.scroll>=previous && fast.model().launcher.list.scroll<=168);
        previous=fast.model().launcher.list.scroll;
    }
    sparse.update(426000);
    CHECK(std::abs(fast.model().launcher.list.scroll-sparse.model().launcher.list.scroll)<0.001f);
    slow.update(450000); fast.update(450000);
    CHECK(slow.model().launcher.list.scroll==84 && fast.model().launcher.list.scroll==168);
    CHECK(!fast.active() && fast.nextUpdate()==INT64_MAX);
    CHECK(!fast.update(1000000));
    for(float speed: {-100000.0f,-1000.0f,0.0f,1000.0f,100000.0f}) {
        TestScreens s; release(s,speed,-200);
        float last=s.model().launcher.list.scroll;
        s.update(430000); const float end=s.model().launcher.list.scroll;
        TestScreens sample; release(sample,speed,-200);
        for(int t=266000;t<=442000;t+=16000) {
            sample.update(t); const float pos=sample.model().launcher.list.scroll;
            CHECK(pos>=0 && pos<=336);
            CHECK(end>=200 ? pos>=last && pos<=end : pos<=last && pos>=end);
            last=pos;
        }
        CHECK(sample.model().screen==ScreenId::AppList);
    }
    TestScreens stopped; release(stopped,1000);
    Events e{}; e.gesture=Gesture::TouchStart; stopped.handle(e,282000);
    const float pos=stopped.model().launcher.list.scroll;
    CHECK(!stopped.active()); stopped.update(350000); CHECK(stopped.model().launcher.list.scroll==pos);
    e.gesture=Gesture::Tap; e.x=234; e.y=234; stopped.handle(e,360000);
    CHECK(!stopped.model().toast); stopped.update(540000);
    CHECK(!stopped.active());
    TestScreens resumed; release(resumed,1000);
    e={}; e.gesture=Gesture::TouchStart; resumed.handle(e,282000);
    const float held=resumed.model().launcher.list.scroll;
    e.gesture=Gesture::DragStart; e.totalY=-15; resumed.handle(e,300000);
    CHECK(std::abs(resumed.model().launcher.list.scroll-held-15)<0.001f);
    e.gesture=Gesture::DragEnd; resumed.handle(e,320000); resumed.update(500000);
    CHECK(!resumed.active() && !resumed.model().toast);
    TestScreens edge; release(edge,100000,-1000); edge.update(450000);
    CHECK(edge.model().launcher.list.scroll==336 && edge.model().screen==ScreenId::AppList);
    TestScreens decide; release(decide,1000);
    e={}; e.decide=true; decide.handle(e,282000); decide.update(462000);
    CHECK(decide.model().toast && decide.model().launcher.list.scroll==decide.model().launcher.list.selection*84);
    TestScreens buttons; release(buttons,1000);
    e={}; e.next=true; buttons.handle(e,282000); buttons.update(462000);
    CHECK(buttons.model().launcher.list.scroll==buttons.model().launcher.list.selection*84);
    e={}; e.home=true; buttons.handle(e,470000);
    CHECK(!buttons.active() && buttons.model().screen==ScreenId::Home);
}
// The launcher's use of the shared list: leaving for a screen mid-animation,
// and presses that must not open anything.
void launcherList() {
    Events e{};
    // Wrap to the stopwatch row with A and open it before the scroll lands:
    // the list arrives at once, leaves no deadline and comes back in place.
    TestScreens s; e.next=true; s.handle(e,0); s.update(180000);
    for(int i=0;i<5;++i) { e={}; e.next=true; s.handle(e,200000+i*40000); s.update(216000+i*40000); }
    CHECK(s.model().launcher.list.selection==0 && s.model().launcher.list.animating);
    e={}; e.decide=true; s.handle(e,380000);
    auto m=s.model();
    CHECK(m.screen==ScreenId::Stopwatch && !m.launcher.list.animating && m.launcher.list.scroll==0 && m.launcher.transition==1);
    CHECK(s.nextUpdate()==INT64_MAX || s.nextUpdate()>380000+ListController::FrameUs);
    e={}; e.home=true; s.handle(e,400000);
    // Opening from a half raised list completes the slide too, so the clock
    // never shows under the opened screen.
    TestScreens half; e={}; e.next=true; half.handle(e,0); half.update(48000);
    CHECK(half.model().launcher.transition>0 && half.model().launcher.transition<1);
    const auto first=layoutListRow(placementOf(half.model()),0);
    e={}; e.gesture=Gesture::Tap; e.x=first.labelX; e.y=first.centerY; half.handle(e,50000);
    CHECK(half.model().screen==ScreenId::Stopwatch && half.model().launcher.transition==1 && !half.active());
    // A vertical scroll ends without deciding the row it passed over.
    TestScreens scroll; e={}; e.next=true; scroll.handle(e,0); scroll.update(180000);
    e={}; e.gesture=Gesture::DragStart; e.totalY=-100; scroll.handle(e,200000);
    e.gesture=Gesture::DragMove; e.totalY=-120; scroll.handle(e,210000);
    e.gesture=Gesture::DragEnd; e.velocityY=0; scroll.handle(e,220000);
    scroll.update(500000);
    CHECK(scroll.model().screen==ScreenId::AppList && !scroll.model().toast && !scroll.active());
    CHECK(scroll.model().launcher.list.scroll==scroll.model().launcher.list.selection*84);
    // Home resets the list to its first row, with nothing left running.
    e={}; e.home=true; scroll.handle(e,600000);
    CHECK(scroll.model().launcher.list.selection==0 && scroll.model().launcher.list.scroll==0 && !scroll.active());
}
// The launcher on its own: the slide, who owns a drag, input that lands while
// the slide is still moving, and what leaving and home keep or reset.
void launcherController() {
    const Viewport v{468,468};
    const float spacing=float(rowSpacing(v));
    Events e{};
    // At rest on the clock: nothing moves and no deadline is left.
    LauncherController clock(v);
    CHECK(!clock.listShown() && !clock.active() && clock.nextUpdate()==INT64_MAX);
    CHECK(clock.model().transition==0 && clock.model().list.selection==0);
    // A tap outside the apps target does nothing; on it, the list comes up.
    e={}; e.gesture=Gesture::Tap; e.x=234; e.y=100;
    CHECK(!clock.handle(e,0).changed && !clock.listShown());
    const Rect apps=appsTarget(v);
    e.x=apps.x+apps.w/2; e.y=apps.y+apps.h/2;
    CHECK(clock.handle(e,0).changed && clock.listShown() && clock.nextUpdate()==ListController::FrameUs);
    // A horizontal drag belongs to nobody.
    LauncherController sideways(v);
    e={}; e.gesture=Gesture::DragStart; e.totalX=100; e.totalY=10;
    CHECK(!sideways.handle(e,0).changed && !sideways.active());
    // B halfway up the slide decides the selected row: the launcher only names
    // it, and leaving for it lands the slide with nothing left running.
    LauncherController rising(v);
    e={}; e.next=true; rising.handle(e,0); rising.update(48000);
    CHECK(rising.transitioning() && rising.active());
    e={}; e.decide=true;
    const auto out=rising.handle(e,50000);
    CHECK(out.changed && out.open && out.target && out.target->id==AppId::Stopwatch);
    CHECK(rising.transitioning());                   // Nothing opened yet.
    rising.suspend();
    CHECK(rising.model().transition==1 && !rising.active() && rising.nextUpdate()==INT64_MAX);
    CHECK(rising.listShown());                       // The list is where the app returns to.
    // A pull down from the top of a list that is still rising follows the
    // finger back towards the clock and lands there.
    LauncherController pulled(v);
    e={}; e.next=true; pulled.handle(e,0); pulled.update(90000);
    const float mid=pulled.model().transition;
    CHECK(mid>0 && mid<1);
    e={}; e.gesture=Gesture::DragStart; e.totalY=60;
    CHECK(pulled.handle(e,100000).changed && pulled.model().transition<mid);
    e.gesture=Gesture::DragEnd; pulled.handle(e,110000);
    CHECK(!pulled.listShown());
    pulled.update(110000+ListController::AnimationUs);
    CHECK(pulled.model().transition==0 && !pulled.active() && pulled.nextUpdate()==INT64_MAX);
    // A shorter pull springs back up to the list.
    LauncherController kept(v);
    e={}; e.next=true; kept.handle(e,0); kept.update(ListController::AnimationUs);
    e={}; e.gesture=Gesture::DragStart; e.totalY=30; kept.handle(e,200000);
    e.gesture=Gesture::DragEnd; kept.handle(e,210000);
    kept.update(210000+ListController::AnimationUs);
    CHECK(kept.listShown() && kept.model().transition==1);
    // Leaving mid-scroll lands the list on its row and keeps it there; home
    // then puts it back at the top.
    LauncherController away(v);
    e={}; e.next=true; away.handle(e,0); away.update(ListController::AnimationUs);
    away.handle(e,200000); away.handle(e,210000); away.update(226000);
    CHECK(away.model().list.selection==2 && away.model().list.animating);
    away.suspend();
    auto m=away.model();
    CHECK(m.list.selection==2 && m.list.scroll==2*spacing && !m.list.animating && away.nextUpdate()==INT64_MAX);
    away.home();
    m=away.model();
    CHECK(!away.listShown() && m.transition==0 && m.list.selection==0 && m.list.scroll==0);
    CHECK(!away.active() && away.nextUpdate()==INT64_MAX);
}
// Where each part of a frame goes, the order the layers are drawn in, and
// when the composition asks for a full repaint (docs/task9/plan-9-4.md 2).
void composition() {
    FrameModel m; m.viewport={468,468};
    auto c=composeFrame(m);
    CHECK(c.clockVisible() && c.home.offsetY==0 && (c.home.clip==Rect{0,0,468,468}));
    CHECK(c.list && !c.settings && !c.external && !c.stopwatch);
    // Sliding up with the list: the clock's clip ends where the list begins.
    m.screen=ScreenId::AppList; m.launcher.transition=0.5f;
    c=composeFrame(m);
    CHECK(c.clockVisible() && c.home.offsetY==-234 && (c.home.clip==Rect{0,0,468,234}));
    const auto list=appListPlacement(m.viewport,m.launcher.transition,0);
    CHECK(list.region.clip.y==c.home.clip.y+c.home.clip.h);
    m.launcher.transition=1;
    CHECK(!clockVisible(m) && composeFrame(m).list);
    // An open screen covers the clock and the list whatever the slide says,
    // and only its own layer shows.
    for (const auto screen:{ScreenId::Settings,ScreenId::External,ScreenId::Stopwatch}) {
        m.screen=screen; m.launcher.transition=0.5f;
        c=composeFrame(m);
        CHECK(!c.clockVisible() && !clockVisible(m) && !c.list);
        CHECK(int(c.settings)+int(c.external)+int(c.stopwatch)==1);
        CHECK(c.settings==(screen==ScreenId::Settings) && c.external==(screen==ScreenId::External));
    }
    // Back to front: the clock, the list, the screens that cover it, and the
    // notice over everything. Each layer exactly once.
    CHECK(FrameLayerCount==6);
    CHECK(FrameOrder[0]==FrameLayer::Home && FrameOrder[1]==FrameLayer::AppList);
    CHECK(FrameOrder[FrameLayerCount-1]==FrameLayer::Toast);
    for (int i=0;i<FrameLayerCount;++i) for (int j=0;j<i;++j) CHECK(FrameOrder[i]!=FrameOrder[j]);
    // Another screen is a full repaint; anything within the same screen is
    // left to the differential plan (or to the layer, for the settings views).
    FrameComposer composer;
    FrameModel f; f.viewport={468,468};
    CHECK(!composer.compose(f).changed);
    f.screen=ScreenId::AppList; CHECK(composer.compose(f).changed);
    f.launcher.transition=1; f.launcher.list.scroll=84; CHECK(!composer.compose(f).changed);
    f.screen=ScreenId::Settings; CHECK(composer.compose(f).changed);
    f.settings.view=SettingsView::Brightness; f.toast="x"; CHECK(!composer.compose(f).changed);
    f.screen=ScreenId::AppList; CHECK(composer.compose(f).changed);
    f.screen=ScreenId::Home; CHECK(composer.compose(f).changed);
    CHECK(!composer.compose(f).changed);
    // The manager's own frames compose the same way: the clock shows only
    // while the launcher does.
    TestScreens s; Events e{}; e.next=true;
    CHECK(clockVisible(s.model()));
    s.handle(e,0); s.update(ListController::AnimationUs);
    CHECK(!clockVisible(s.model()) && composeFrame(s.model()).list);
    e={}; e.decide=true; s.handle(e,200000);
    CHECK(s.model().screen==ScreenId::Stopwatch && !clockVisible(s.model()) && !composeFrame(s.model()).list);
    e={}; e.home=true; s.handle(e,300000);
    CHECK(clockVisible(s.model()) && composeFrame(s.model()).home.offsetY==0);
}
// The shared list at counts other than the launcher's five, including none.
void listLayout() {
    for(int side: {400,466,468}) {
        const Viewport v{side,side};
        CHECK(maxVisibleListRows(v)<=ListVisibleSlots);
        CHECK(labelWidth(v,false)==labelWidth(v,true)+labelOffset(v)+iconRadius(v));
        for(int count: {0,1,2,5,7,12,40}) {
            const int maxScroll=std::max(0,count-1)*rowSpacing(v);
            for(float transition: {0.0f,0.3f,1.0f})
            for(int scroll=0;scroll<=maxScroll;scroll+=7) {
                const auto p=appListPlacement(v,transition,float(scroll));
                int first=0,last=-1;
                const bool any=visibleListRows(p,count,first,last);
                // Exactly the rows the layout gives a box, and never more
                // than the view keeps slots for.
                int visible=0;
                for(int i=0;i<count;++i) {
                    const auto icon=layoutListRow(p,i,true),plain=layoutListRow(p,i,false);
                    CHECK(icon.box==plain.box);
                    if(icon.box.empty()) { CHECK(!any || i<first || i>last); continue; }
                    ++visible;
                    CHECK(any && i>=first && i<=last);
                    CHECK(icon.labelX==icon.iconX+labelOffset(v) && plain.labelX==icon.iconX-icon.radius);
                    // Drawing and hit testing share the box.
                    CHECK(hitListRow(p,count,icon.box.x+icon.box.w/2,icon.box.y+icon.box.h/2)==i);
                    CHECK(hitListRow(p,count,icon.box.x+icon.box.w,icon.box.y+icon.box.h/2)!=i);
                }
                CHECK(any==(visible>0) && (!any || last-first+1==visible));
                CHECK(visible<=maxVisibleListRows(v));
                if(count==0) CHECK(!any && hitListRow(p,count,side/2,side/2)==-1);
            }
        }
    }
    // A clip of nothing shows nothing, whatever the count.
    int first=0,last=-1;
    CHECK(!visibleListRows(appListPlacement({468,468},0,0),5,first,last));
}
void listController() {
    const Viewport v{468,468};
    const float spacing=float(rowSpacing(v));
    std::array<ListRow,12> storage{};
    for(int i=0;i<12;++i) { storage[i].id=RowId(100+i); storage[i].label="row"; }
    auto rows=[&](int count) { return ListRows{storage.data(),count}; };
    const auto rest=fullListPlacement(v,0);
    // Nothing to select, decide or scroll, and no deadline left behind.
    ListController empty; empty.resize(v); empty.setRows(rows(0));
    CHECK(empty.selection()==-1 && !empty.next(0) && !empty.decide(0).changed);
    CHECK(!empty.tap(rest,234,234,0).changed);
    empty.dragStart(); CHECK(empty.dragMove(-300) && empty.scroll()==0);
    empty.dragEnd(-5000,0); empty.update(1000000);
    CHECK(empty.scroll()==0 && !empty.active() && empty.nextUpdate()==INT64_MAX);
    // One row: A stays on it and nothing ever scrolls.
    ListController one; one.resize(v); one.setRows(rows(1));
    CHECK(one.selection()==0 && one.next(0) && one.selection()==0 && !one.active());
    one.dragStart(); one.dragMove(-200); CHECK(one.scroll()==0);
    one.dragEnd(3000,0); CHECK(one.nextUpdate()==INT64_MAX);
    auto d=one.decide(0); CHECK(d.decided && d.id==100 && d.index==0);
    // Seven rows: A walks to the end and wraps to the first, each time
    // scrolled to where the row is centred.
    ListController seven; seven.resize(v); seven.setRows(rows(7));
    TimeUs now=0;
    for(int i=1;i<=7;++i) {
        CHECK(seven.next(now) && seven.selection()==i%7);
        now+=ListController::AnimationUs; seven.update(now);
        CHECK(seven.scroll()==(i%7)*spacing && seven.nextUpdate()==INT64_MAX);
        const auto hit=seven.tap(fullListPlacement(v,seven.scroll()),300,v.height/2,now);
        CHECK(hit.decided && hit.index==i%7 && hit.id==RowId(100+i%7));
    }
    // Rapid presses retarget from wherever the animation has got to.
    seven.reset(); seven.next(0); seven.next(10000); seven.next(20000); seven.update(30000);
    CHECK(seven.selection()==3 && seven.scroll()<3*spacing);
    seven.update(20000+ListController::AnimationUs); CHECK(seven.scroll()==3*spacing);
    // A row that may not be decided is still selected, and yields no decision.
    storage[3].enabled=false;
    d=seven.decide(0); CHECK(d.changed && !d.decided && d.index==3);
    d=seven.tap(fullListPlacement(v,seven.scroll()),300,v.height/2,0);
    CHECK(d.changed && !d.decided && seven.selection()==3);
    storage[3].enabled=true;
    // A drag scrolls, clamps to the rows there are, and never decides.
    seven.reset(); seven.dragStart(); seven.dragMove(-10000);
    CHECK(seven.scroll()==6*spacing && seven.selection()==6 && !seven.next(0) && !seven.decide(0).changed);
    seven.dragEnd(0,0); CHECK(!seven.active());
    // A touch while coasting stops it; the release then settles, never decides.
    seven.reset(); seven.dragStart(); seven.dragMove(-60); seven.dragEnd(1000,0);
    seven.update(16000); CHECK(seven.settling());
    const float held=seven.scroll();
    CHECK(seven.touchStart() && !seven.active() && seven.nextUpdate()==INT64_MAX);
    seven.update(100000); CHECK(seven.scroll()==held);
    CHECK(seven.releaseAfterStop(100000) && !seven.releaseAfterStop(100000));
    seven.update(100000+ListController::AnimationUs);
    CHECK(seven.scroll()==seven.selection()*spacing && !seven.active());
    // A touch that finds it still is an ordinary press.
    CHECK(!seven.touchStart() && !seven.releaseAfterStop(0));
    // A tap lands on whatever row is under it mid-scroll: the hit test uses
    // the same placement the frame was drawn with.
    seven.reset(); seven.next(0); seven.update(90000);
    const auto moving=fullListPlacement(v,seven.scroll());
    const auto row=layoutListRow(moving,0);
    CHECK(seven.tap(moving,row.labelX,row.centerY,90000).index==0);
    // Leaving: finish arrives at once and leaves no deadline; cancel aligns.
    seven.reset(); seven.next(0); seven.update(16000); seven.finish();
    CHECK(seven.scroll()==spacing && seven.nextUpdate()==INT64_MAX && !seven.active());
    seven.dragStart(); seven.dragMove(-30); seven.cancel(0);
    CHECK(!seven.state().dragging && seven.active());
    seven.update(ListController::AnimationUs); CHECK(seven.scroll()==seven.selection()*spacing);
    // Rows change: the selection follows its id, or falls back into range.
    seven.reset(); for(int i=0;i<4;++i) seven.next(0);
    seven.finish(); CHECK(seven.selection()==4 && seven.scroll()==4*spacing);
    std::swap(storage[4],storage[1]); seven.setRows(rows(7));
    CHECK(seven.selection()==1 && seven.scroll()==spacing && seven.state().selection==1);
    seven.setRows(rows(1)); CHECK(seven.selection()==0 && seven.scroll()==0);
    seven.setRows(rows(0)); CHECK(seven.selection()==-1 && !seven.active());
    seven.setRows(rows(3)); CHECK(seven.selection()==0);
    std::swap(storage[4],storage[1]);
    // Handing a press to the owner forgets the stop, so its release is the
    // owner's to act on.
    seven.setRows(rows(7)); seven.dragStart(); seven.dragMove(-60); seven.dragEnd(1000,0);
    seven.update(16000); CHECK(seven.touchStart());
    seven.handOff(); CHECK(!seven.releaseAfterStop(0) && !seven.active());
}
struct Platform : Hal,RenderPort,DisplayDataSource {
    TimeUs time=0; InputSnapshot input{}; int draws=0,invalidations=0,samples=0;
    bool valid=false; UsbState usb{};
    TimeUs now() override { return time; }
    InputSnapshot sampleInput() override { return input; }
    UsbState sampleUsb() override { return usb; }
    void setScreenOff(bool) override {}
    void waitUs(TimeUs us) override { time+=us; }
    bool inputPending() override { return input.a||input.b||input.touching; }
    bool readRtc(CivilTime&) override { return false; }
    bool writeRtc(const CivilTime&) override { return false; }
    void setUtcClock(int64_t) override {}
    int64_t utcClockUs() override { return 0; }
    BatteryState sampleBattery() override { return {}; }
    void setBrightness(int) override {}
    void invalidate() override { ++invalidations; }
    void draw(const FrameModel&,const WatchData&) override { ++draws; }
    WatchData sample(TimeUs now) override {
        ++samples; WatchData d; d.timeValid=valid;
        d.localTime.tm_sec=(now/1000000)%60; d.subsecondUs=now%1000000; return d;
    }
    TimeUs nextUpdate(TimeUs now,const WatchData& d) const override { return nextMinute(now,d); }
};
void deadlines() {
    Platform p; p.valid=true;
    Application application(p,p,p,468,468); auto& r=application.runtime(); r.begin(); r.step(); CHECK(p.draws==1);
    p.input.a=true; // held input prevents sleep; no short press yet
    for(int i=0;i<5999;++i) { p.time+=10000; r.step(); }
    CHECK(p.draws==1 && p.samples==1);
    p.time=60000000; r.step(); CHECK(p.draws==2);
    p.usb={true,5000,true}; p.time+=1000000; r.step(); CHECK(p.draws==2);
    r.dataChanged(); r.step(); CHECK(p.draws==3);
    p.input={}; p.time+=10000; r.step(); // release opens list
    p.time+=180000; r.step();
    const int count=p.draws;
    p.input.b=true; // hold on the list past the clock boundary
    for(int i=0;i<7000;++i) { p.time+=10000; r.step(); }
    CHECK(p.draws==count);
    p.input={true,true}; p.time+=10000; r.step(); p.time+=600000; r.step();
    CHECK(r.model().screen==ScreenId::Home);
    p.input={}; p.time+=10000; r.step();
    p.time+=30000000; r.step(); CHECK(r.power().screenOff());
    const int off=p.draws, samples=p.samples;
    for(int i=0;i<7000;++i) { p.time+=10000; r.step(); }
    CHECK(p.draws==off && p.samples==samples);
    p.input={false,false,true,234,390}; p.time+=10000; r.step();
    CHECK(p.draws==off+1 && p.invalidations>=3 && r.model().screen==ScreenId::Home);
    p.input={}; p.time+=10000; r.step(); CHECK(p.draws==off+1);
    WatchData d; CHECK(nextMinute(0,d)==INT64_MAX);
    d.timeValid=true; d.localTime.tm_sec=59; d.subsecondUs=999999;
    CHECK(nextMinute(999,d)==1000);
}
void repaint() {
    constexpr int W=48,H=48,N=8;
    using Pixels=std::array<uint16_t,W*H>;
    Pixels partial{},full{};
    std::array<Element,N> elements{};
    FramePlan plan;
    std::mt19937 random(1234);
    auto fill=[](Pixels& pixels,Rect rect,uint16_t color) {
        rect=intersect(rect,{0,0,W,H});
        for(int y=rect.y;y<rect.y+rect.h;++y) for(int x=rect.x;x<rect.x+rect.w;++x) pixels[y*W+x]=color;
    };
    bool invalidate=true;
    std::array<Rect,N> boxes{};
    std::array<uint16_t,N> colors{};
    for(int step=0;step<3000;++step) {
        // Most objects remain unchanged, while moving/deleting a lower layer
        // can invalidate an unchanged upper layer.
        int index=random()%N;
        boxes[index]={int(random()%60)-12,int(random()%60)-12,int(random()%25),int(random()%25)};
        if(step%11==0) boxes[index]={};
        colors[index]=1+random()%65000;
        const bool overflow=step%71==0;
        plan.begin(invalidate,overflow ? 2 : FramePlan::Capacity);
        std::array<int,N> handles{};
        for(int i=0;i<N;++i) handles[i]=plan.add(elements[i],intersect(boxes[i],{0,0,W,H}),colors[i]);
        plan.resolve();
        CHECK(plan.overflow()==overflow);
        if(plan.full()) partial.fill(0);
        else for(int i=0;i<plan.count();++i) fill(partial,plan.eraseBox(i),0);
        for(int i=0;i<N;++i) if(plan.shouldPaint(handles[i])) fill(partial,boxes[i],colors[i]);
        full.fill(0); for(int i=0;i<N;++i) fill(full,boxes[i],colors[i]);
        CHECK(partial==full);
        // The statistics overlay skips its push when it sits outside this box,
        // so every pixel the differential pass touched has to be inside it.
        if(!plan.full()) {
            const Rect dirty=plan.dirtyBounds();
            auto inside=[&](Rect r) {
                r=intersect(r,{0,0,W,H});
                return r.empty() || (!dirty.empty() && r.x>=dirty.x && r.y>=dirty.y &&
                    r.x+r.w<=dirty.x+dirty.w && r.y+r.h<=dirty.y+dirty.h);
            };
            for(int i=0;i<plan.count();++i) CHECK(inside(plan.eraseBox(i)));
            for(int i=0;i<N;++i) if(plan.shouldPaint(handles[i])) CHECK(inside(boxes[i]));
        }
        invalidate=plan.overflow();
    }
    plan.begin(false);
    for(int i=0;i<N;++i) plan.add(elements[i],intersect(boxes[i],{0,0,W,H}),colors[i]);
    plan.resolve(); CHECK(!plan.anyPaint());
    // A part registered last (a notice, a list out of slots) can still turn
    // the whole frame into a full repaint of everything already registered.
    plan.begin(false);
    std::array<int,N> handles{};
    for(int i=0;i<N;++i) handles[i]=plan.add(elements[i],intersect(boxes[i],{0,0,W,H}),colors[i]);
    plan.forceFull(); plan.resolve();
    CHECK(plan.full() && plan.anyPaint());
    for(int i=0;i<N;++i) CHECK(plan.shouldPaint(handles[i]));
}
int main() {
    navigation(); flick(); launcherList(); launcherController(); composition();
    listLayout(); listController(); deadlines(); repaint();
    std::cout<<"PASS: navigation/geometry, launcher controller, frame composition, "
               "shared list layout/controller, display deadlines, "
               "3000 differential framebuffer cases with dirty bounds\n";
}
