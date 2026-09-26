#include "host/HostApplication.h"
#include "TestScreens.h"
#include "host/FrameComposer.h"
#include "features/launcher/AppListLayout.h"
#include "ui/rendering/Element.h"
#include "ui/rendering/PaintContext.h"
#include "ui/list/ListController.h"
#include "ui/list/ListLayout.h"
#include "ui/graphics/MaskImage.h"
#include "ui/graphics/VlwGlyphs.h"
#include <fstream>
#include <iterator>
#include <vector>
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
    CHECK((composeFrame(s.model()).home.listProgress==0 &&
           composeFrame(s.model()).home.clip==Rect{0,0,468,468}));
    drag(-51); CHECK(s.model().screen==ScreenId::AppList && s.model().launcher.transition==1);
    CHECK(composeFrame(s.model()).home.listProgress==1 && composeFrame(s.model()).home.clip.empty());
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
    // A tap on the clock is the watch face's, even on APPS: the launcher only
    // opens the list when the face asks, and a repeated request is not a
    // second slide.
    CHECK(clock.atRest());
    const Rect apps=digitalAppsTarget(v,0);
    e={}; e.gesture=Gesture::Tap; e.x=apps.x+apps.w/2; e.y=apps.y+apps.h/2;
    CHECK(!clock.handle(e,0).changed && !clock.listShown() && clock.atRest());
    CHECK(clock.openList(0) && clock.listShown() && clock.nextUpdate()==ListController::FrameUs);
    CHECK(!clock.atRest() && !clock.openList(10000));
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
    CHECK(out.changed && out.open && out.target && out.target->id==LaunchTargetId::Stopwatch);
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
    CHECK(c.clockVisible() && c.home.listProgress==0 && (c.home.clip==Rect{0,0,468,468}));
    CHECK(c.list && !c.settings && !c.external && !c.stopwatch);
    // The list rising: the clock is told the progress and the part left
    // uncovered, and is not moved by the system (docs/task10/plan-10-4.md 5).
    // The clock's clip ends where the list, its background and rows, begin.
    m.screen=ScreenId::AppList;
    for (int step=0;step<=40;++step) {
        m.launcher.transition=float(step)/40;
        c=composeFrame(m);
        const auto list=appListPlacement(m.viewport,m.launcher.transition,0);
        const Rect cover=appListCover(m.viewport,m.launcher.transition);
        CHECK(c.home.listProgress==m.launcher.transition && c.home.viewport.height==468);
        CHECK(c.home.clip.y==0 && c.home.clip.w==468 && cover.y==c.home.clip.h);
        CHECK(list.region.clip==cover && cover.y+cover.h==468 && cover.w==468);
        CHECK(c.clockVisible()==(step<40));
    }
    m.launcher.transition=0.5f;
    c=composeFrame(m);
    CHECK(c.clockVisible() && (c.home.clip==Rect{0,0,468,234}));
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
    // Back to front: the clock, the list's background and rows, the screens
    // that cover it, and the notice over everything. Each layer exactly once.
    CHECK(FrameLayerCount==7);
    CHECK(FrameOrder[0]==FrameLayer::Home && FrameOrder[1]==FrameLayer::AppListBackground &&
          FrameOrder[2]==FrameLayer::AppList);
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
    CHECK(clockVisible(s.model()) && composeFrame(s.model()).home.listProgress==0);
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
    HostApplication application(p,p,p,468,468); auto& r=application.runtime(); r.begin(); r.step(); CHECK(p.draws==1);
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
// The differential frame against a full repaint (docs/task10/plan-10-4.md 1):
// a multicoloured background with a band that moves and changes colour, as
// a face's scenery does, and elements over it that move, change, vanish and
// overlap. Each frame restores the damage and repaints inside it only.
void repaint() {
    constexpr int W=48,H=48,N=8;
    const Rect screen{0,0,W,H};
    using Pixels=std::array<uint16_t,W*H>;
    Pixels partial{},full{};
    std::array<Element,N> elements{};
    FramePlan plan;
    std::mt19937 random(1234);
    auto fill=[](Pixels& pixels,Rect rect,uint16_t color) {
        rect=intersect(rect,{0,0,W,H});
        for(int y=rect.y;y<rect.y+rect.h;++y) for(int x=rect.x;x<rect.x+rect.w;++x) pixels[y*W+x]=color;
    };
    Rect band{0,20,W,8},shownBand=band;
    uint16_t bandColor=0x07e0,shownBandColor=bandColor;
    // The base the Renderer restores, then the background layer: stripes
    // everywhere, the band over them. `area` clips it as the context does.
    auto background=[&](Pixels& pixels,Rect area) {
        area=intersect(area,screen);
        for(int y=area.y;y<area.y+area.h;++y) for(int x=area.x;x<area.x+area.w;++x)
            pixels[y*W+x]=uint16_t(0x1000+((x/5+y/3)%4)*0x0421);
        fill(pixels,intersect(band,area),bandColor);
    };
    bool invalidate=true;
    std::array<Rect,N> boxes{},shown{};
    std::array<uint16_t,N> colors{},shownColors{};
    bool seen=false;
    for(int step=0;step<3000;++step) {
        // Most objects remain unchanged, while moving/deleting a lower layer
        // must restore an unchanged upper layer.
        int index=random()%N;
        boxes[index]={int(random()%60)-12,int(random()%60)-12,int(random()%25),int(random()%25)};
        if(step%11==0) boxes[index]={};
        colors[index]=1+random()%65000;
        if(step%13==0) band.y=int(random()%40);
        if(step%29==0) bandColor=uint16_t(random());
        const bool overflow=step%71==0;
        plan.begin(invalidate,screen,overflow ? 2 : FramePlan::Capacity);
        // The background declares what it changed, as a face does.
        const bool scenery=band!=shownBand || bandColor!=shownBandColor;
        if(scenery) plan.damage(unite(band,shownBand));
        std::array<int,N> handles{};
        for(int i=0;i<N;++i) handles[i]=plan.add(elements[i],intersect(boxes[i],screen),colors[i]);
        plan.resolve();
        CHECK(plan.overflow()==overflow);
        const Rect area=plan.area();
        if(plan.full()) CHECK(area==screen);
        else {
            // Exactly the changes: nothing an unchanged element overlaps.
            Rect expected=scenery ? unite(band,shownBand) : Rect{};
            for(int i=0;i<N;++i) {
                const Rect now=intersect(boxes[i],screen),before=seen ? intersect(shown[i],screen) : Rect{};
                if(!seen || now!=before || colors[i]!=shownColors[i]) expected=unite(expected,unite(now,before));
            }
            expected=intersect(expected,screen);
            CHECK(area==expected || (area.empty() && expected.empty()));
        }
        background(partial,area);
        for(int i=0;i<N;++i) {
            CHECK(plan.shouldPaint(handles[i])==(plan.full() || intersect(boxes[i],screen).intersects(area)));
            fill(partial,intersect(boxes[i],area),colors[i]);
        }
        background(full,screen); for(int i=0;i<N;++i) fill(full,boxes[i],colors[i]);
        CHECK(partial==full);
        shown=boxes; shownColors=colors; seen=true; shownBand=band; shownBandColor=bandColor;
        invalidate=plan.overflow();
    }
    plan.begin(false,screen);
    for(int i=0;i<N;++i) plan.add(elements[i],intersect(boxes[i],screen),colors[i]);
    plan.resolve(); CHECK(!plan.anyPaint() && plan.area().empty());
    // A part registered last (a list out of slots, a face just selected) can
    // still turn the whole frame into a full repaint of everything already
    // registered.
    plan.begin(false,screen);
    std::array<int,N> handles{};
    for(int i=0;i<N;++i) handles[i]=plan.add(elements[i],intersect(boxes[i],screen),colors[i]);
    plan.forceFull(); plan.resolve();
    CHECK(plan.full() && plan.anyPaint() && plan.area()==screen);
    for(int i=0;i<N;++i) CHECK(plan.shouldPaint(handles[i]));
}
// The damage rectangle case by case, the context's clip, and the list's
// background (docs/task10/plan-10-4.md 7).
struct ClipProbe {
    Rect clip{}; int calls=0;
    void setClipRect(int x,int y,int w,int h) { clip={x,y,w,h}; ++calls; }
};
void damage() {
    const Rect screen{0,0,468,468};
    FramePlan plan;
    Element scenery,digit,chip,apps;
    const Rect digitBox{300,200,60,70},chipBox{100,290,120,48},appsBox{200,360,68,50};
    auto frame=[&](Rect d,Rect c,Rect a,uint32_t value,bool invalidate=false) {
        plan.begin(invalidate,screen);
        // A foreground as large as the panel that never changes, registered
        // first: under the old overlap closure it pulled every change into a
        // full repaint.
        const int s=plan.add(scenery,screen,1);
        plan.add(digit,d,value); plan.add(chip,c,7); plan.add(apps,a,9);
        plan.resolve();
        return s;
    };
    frame(digitBox,chipBox,appsBox,0,true);
    CHECK(plan.full() && plan.area()==screen);
    // A small change over it: the damage is the digit, not the panel, and the
    // large element repaints inside it.
    int s=frame(digitBox,chipBox,appsBox,1);
    CHECK(!plan.full() && plan.area()==digitBox && plan.shouldPaint(s));
    frame(digitBox,chipBox,appsBox,1); CHECK(!plan.anyPaint());
    // Only the new box (an element growing or appearing), only the old one
    // (vanishing), the new one again (coming back).
    const Rect wider{digitBox.x-10,digitBox.y,digitBox.w+10,digitBox.h};
    frame(wider,chipBox,appsBox,1); CHECK(plan.area()==wider);
    frame(wider,{},appsBox,1); CHECK(plan.area()==chipBox);
    frame(wider,chipBox,appsBox,1); CHECK(plan.area()==chipBox);
    // Two changes far apart: one rectangle around both, and whatever lies
    // between them repaints too.
    const Rect lowered{appsBox.x,appsBox.y+4,appsBox.w,appsBox.h};
    frame(digitBox,chipBox,lowered,1);
    CHECK(plan.area()==unite(wider,unite(appsBox,lowered)));
    // The edge of antialiasing is part of the box a part registers, so it is
    // restored with it: the damage is never narrower than the box.
    const Rect padded{digitBox.x-2,digitBox.y-2,digitBox.w+4,digitBox.h+4};
    frame(padded,chipBox,lowered,1);
    CHECK(plan.area()==padded);
    // Damage off the screen is cut at its edge; a background change alone
    // makes a frame.
    plan.begin(false,screen); plan.damage({-20,440,60,60}); plan.resolve();
    CHECK((plan.area()==Rect{0,440,40,28}));
    plan.begin(false,screen); plan.damage({500,500,10,10}); plan.resolve();
    CHECK(!plan.anyPaint());
    // Overflow: the whole screen, every element, including a refused one.
    plan.begin(false,screen,2);
    plan.add(scenery,screen,1); plan.add(digit,digitBox,1);
    CHECK(plan.add(chip,chipBox,7)==-1);
    plan.resolve();
    CHECK(plan.full() && plan.overflow() && plan.area()==screen && plan.shouldPaint(-1));
    // The context: the three-way intersection, and nothing set when empty.
    plan.begin(false,screen); plan.damage({100,100,100,100}); plan.resolve();
    const PaintContext context(plan,plan.area());
    ClipProbe probe;
    CHECK(context.clip(probe,{150,150,100,20}) && (probe.clip==Rect{150,150,50,20}));
    const auto narrowed=context.within({0,0,468,160});
    CHECK(narrowed.clip(probe,{150,150,100,20}) && (probe.clip==Rect{150,150,50,10}));
    CHECK(!narrowed.clip(probe,{0,300,468,20}) && probe.calls==2);
    CHECK(!context.within({}).clip(probe,screen) && probe.calls==2);
    narrowed.restore(probe); CHECK((probe.clip==Rect{100,100,100,60}));
    // The list's background: the band its edge swept, all of it for a new
    // colour, nothing when neither moved.
    const Viewport v{468,468};
    const Rect half=appListCover(v,0.5f),more=appListCover(v,0.75f);
    CHECK((appListCoverChange(half,0,more,0)==Rect{0,more.y,468,half.y-more.y}));
    CHECK((appListCoverChange(more,0,half,0)==Rect{0,more.y,468,half.y-more.y}));
    CHECK(appListCoverChange(half,0,half,0).empty());
    CHECK(appListCoverChange(half,0,half,0x1234)==half);
    CHECK(appListCoverChange(half,0,more,0x1234)==more);
    CHECK(appListCoverChange({},0,half,0)==half && appListCoverChange(half,0,{},0)==half);
    CHECK(appListCover(v,0).empty() && appListCover(v,1)==screen && appListUncovered(v,1).empty());
}
// Work 10-2: the clock's taps and long presses belong to the watch face.
Events gesture(Gesture g,int x,int y) { Events e{}; e.gesture=g; e.x=x; e.y=y; return e; }
void homeInput() {
    const Viewport v{468,468};
    const auto layout=digitalLayout(v,DigitalVariant::HourMinute,0);
    // The target covers what Digital draws for APPS, and nothing of the time.
    CHECK(layout.apps.contains(layout.appsIcon.x,layout.appsIcon.y));
    CHECK(layout.apps.contains(layout.cx,layout.appsBaseline-5));
    CHECK(!layout.apps.contains(layout.cx,layout.timeBaseline));
    TestScreens s; TimeUs now=0;
    const int ax=layout.apps.x+layout.apps.w/2,ay=layout.apps.y+layout.apps.h/2;
    // Outside APPS: the face hears it and asks for nothing.
    CHECK(s.homeAtRest());
    CHECK(!s.handle(gesture(Gesture::Tap,234,100),now));
    CHECK(s.home.events==1 && s.home.last.kind==HomeEventKind::Tap && s.home.last.x==234 && s.home.last.y==100);
    CHECK(s.model().screen==ScreenId::Home);
    // A long press anywhere, APPS too, is the variant; the list stays shut.
    CHECK(s.handle(gesture(Gesture::LongPress,ax,ay),now));
    CHECK(s.home.last.kind==HomeEventKind::LongPress && s.home.digital.variant()==DigitalVariant::HourMinuteSecond);
    CHECK(s.model().screen==ScreenId::Home && s.homeAtRest());
    // On APPS: the list opens with the buttons' slide.
    CHECK(s.handle(gesture(Gesture::Tap,ax,ay),now));
    CHECK(s.model().screen==ScreenId::AppList && !s.homeAtRest() && s.active());
    // Not at rest: the list's gestures stay the list's.
    const int heard=s.home.events;
    s.handle(gesture(Gesture::LongPress,ax,ay),now);
    CHECK(s.home.events==heard);
    now+=400000; s.update(now);
    // On the way back to the clock nothing reaches the face until it settles.
    Events e{}; e.home=true; s.handle(e,now);
    CHECK(s.homeAtRest());
    e={}; e.next=true; s.handle(e,now);
    now+=50000; s.update(now);
    e={}; e.gesture=Gesture::DragStart; e.totalY=60; s.handle(e,now);
    e.gesture=Gesture::DragEnd; s.handle(e,now);           // back down: returning
    CHECK(s.model().screen==ScreenId::Home && s.model().launcher.transition>0 && !s.homeAtRest());
    s.handle(gesture(Gesture::Tap,ax,ay),now);
    CHECK(s.home.events==heard && s.model().screen==ScreenId::Home);
    now+=400000; s.update(now);
    CHECK(s.homeAtRest());
    // An open screen is never the clock at rest, and gets its own taps.
    e={}; e.next=true; s.handle(e,now); now+=400000; s.update(now);
    e={}; e.decide=true; s.handle(e,now);
    CHECK(s.model().screen==ScreenId::Stopwatch && !s.homeAtRest());
    s.handle(gesture(Gesture::Tap,ax,ay),now);
    CHECK(s.home.events==heard);
    // Without a face bound, a tap on the clock does nothing; A still opens the list.
    AppState state; ScreenManager bare(state.stopwatch,state.runtime,468,468);
    CHECK(!bare.handle(gesture(Gesture::Tap,ax,ay),0) && bare.model().screen==ScreenId::Home);
    e={}; e.next=true; CHECK(bare.handle(e,0) && bare.model().screen==ScreenId::AppList);
}
void digitalControl() {
    const Viewport v{468,468};
    DigitalControl d;
    WatchData w; w.timeValid=true; w.localTime.tm_sec=58; w.subsecondUs=250000;
    CHECK(d.variant()==DigitalVariant::HourMinute);
    CHECK(d.nextUpdate(1000,w)==1000+1750000);           // 2s - 0.25s to the minute
    HomeEvent press; press.kind=HomeEventKind::LongPress;
    const auto out=d.handle(press,v);
    CHECK(out.changed && out.request==HomeRequest::None && d.variant()==DigitalVariant::HourMinuteSecond);
    CHECK(d.nextUpdate(1000,w)==1000+750000);            // the next second
    w.localTime.tm_sec=0; w.subsecondUs=0;
    CHECK(d.nextUpdate(1000,w)==1000+1000000);           // on a boundary: the next one
    WatchData unset;                                     // an unset clock asks for nothing
    CHECK(d.nextUpdate(1000,unset)==INT64_MAX);
    d.handle(press,v);
    CHECK(d.variant()==DigitalVariant::HourMinute && d.nextUpdate(1000,unset)==INT64_MAX);
    // Taps change nothing of the face; only APPS asks for the list.
    HomeEvent tap; tap.x=234; tap.y=100;
    CHECK(!d.handle(tap,v).changed && d.handle(tap,v).request==HomeRequest::None);
    tap.y=390;
    CHECK(!d.handle(tap,v).changed && d.handle(tap,v).request==HomeRequest::OpenAppList);
    // The first two items are drawn, in the providers' order, so only their
    // deadlines wake the display.
    WatchData three; three.background.count=3;
    three.background.items[0].appId=LaunchTargetId::External2;
    three.background.items[1].appId=LaunchTargetId::Stopwatch;
    three.background.items[2].appId=LaunchTargetId::External1;
    const auto shown=d.backgroundInterest(three.background);
    CHECK(shown.count==2 && shown.ids[0]==LaunchTargetId::External2 && shown.ids[1]==LaunchTargetId::Stopwatch);
    // With items the APPS target moves down with the drawing, and the hit
    // test follows the frame the face was last given.
    CHECK(digitalAppsTarget(v,0).y<digitalAppsTarget(v,1).y && digitalAppsTarget(v,1)==digitalAppsTarget(v,2));
    tap.y=digitalAppsTarget(v,0).y+2;
    CHECK(d.handle(tap,v).request==HomeRequest::OpenAppList);
    d.update(three); CHECK(d.items()==2);
    CHECK(d.handle(tap,v).request==HomeRequest::None);
    tap.y=digitalAppsTarget(v,2).y+digitalAppsTarget(v,2).h-2;
    CHECK(d.handle(tap,v).request==HomeRequest::OpenAppList);
    d.update(WatchData{}); CHECK(d.items()==0 && d.handle(tap,v).request==HomeRequest::None);
    // A smaller panel scales the target with the drawing.
    CHECK(digitalAppsTarget({233,233},0)==(Rect{84,177,64,35}));
}
// Every part of Digital inside the round panel, the time in groups that tile
// its row, and the chips in order (docs/task10/plan-10-3.md 3).
bool insideCircle(const Viewport& v,const Rect& r) {
    const float cx=v.width/2.0f,cy=v.height/2.0f,radius=std::min(v.width,v.height)/2.0f-1;
    for (int x:{r.x,r.x+r.w}) for (int y:{r.y,r.y+r.h})
        if ((x-cx)*(x-cx)+(y-cy)*(y-cy)>radius*radius) return false;
    return true;
}
void digitalLayoutRules() {
    const Viewport v{466,466};
    for (auto variant:{DigitalVariant::HourMinute,DigitalVariant::HourMinuteSecond})
        for (int items=0;items<=DigitalMaxItems;++items) {
            const auto l=digitalLayout(v,variant,items);
            const bool seconds=variant==DigitalVariant::HourMinuteSecond;
            CHECK(insideCircle(v,l.hour) && insideCircle(v,l.minute) && insideCircle(v,l.appsIcon) && insideCircle(v,l.apps));
            // The groups meet edge to edge: a digit changing in one never
            // spills into the next, and the colons stay where they are.
            CHECK(l.hour.x+l.hour.w==l.minute.x && l.hour.y==l.minute.y && l.hour.h==l.minute.h);
            CHECK(l.colon1X==l.hourRight && l.minute.x==l.colon1X+22);
            if (seconds) {
                CHECK(l.minute.x+l.minute.w==l.second.x && l.second.x==l.secondX && insideCircle(v,l.second));
                CHECK(l.secondX-(l.hourRight-114)==114+22+115+22);   // 388 wide in all
                CHECK(l.colon2X==l.secondX-22 && l.minuteX==l.minute.x+115/2);
            } else {
                CHECK(l.second.empty() && l.minuteX==l.minute.x);
                CHECK(l.minute.x+115-(l.hourRight-114)==251);
            }
            // The ink sits inside the boxes: 71 above the baseline, 1 below.
            CHECK(l.hour.y<=l.timeBaseline-71 && l.hour.y+l.hour.h>=l.timeBaseline+1);
            // The reference rows, and room made for the items.
            CHECK(l.timeBaseline==(items ? 242 : 270) && l.batteryY==(items ? 61 : 79));
            CHECK(l.apps==digitalAppsTarget(v,items) && l.apps.contains(l.appsIcon.x,l.appsIcon.y));
            CHECK(l.hour.y+l.hour.h<l.chipY-l.chipHeight/2 || !items);
            // Chips: the widest allowed, side by side, stay on the panel.
            if (items) {
                const int limit=digitalChipWidthLimit(l,items);
                const int widths[DigitalMaxItems]={limit,limit};
                Rect chips[DigitalMaxItems];
                CHECK(placeDigitalChips(l,widths,items,chips)==items);
                for (int i=0;i<items;++i) {
                    CHECK(insideCircle(v,chips[i]) && chips[i].h==48 && chips[i].y+24==312);
                    CHECK(!chips[i].intersects(l.hour) && !chips[i].intersects(l.apps));
                }
                if (items==2) CHECK(chips[1].x==chips[0].x+chips[0].w+16);
            }
        }
    // The reference chips: 120 wide each, centred as a pair, left to right.
    const auto l=digitalLayout(v,DigitalVariant::HourMinute,2);
    const int widths[]={120,120};
    Rect chips[2];
    placeDigitalChips(l,widths,2,chips);
    CHECK(chips[0]==(Rect{105,288,120,48}) && chips[1]==(Rect{241,288,120,48}));
    // One chip is centred alone; more items than fit count as two.
    placeDigitalChips(l,widths,1,chips);
    CHECK(chips[0].x==173);
    CHECK(digitalLayout(v,DigitalVariant::HourMinute,5).timeBaseline==242);
    CHECK(placeDigitalChips(l,widths,5,chips)==2);
    // Other metrics (a built-in fallback font) move the groups with them.
    DigitalMetrics wide; wide.hourWidth=120; wide.minuteWidth=120; wide.colonWidth=26;
    const auto f=digitalLayout(v,DigitalVariant::HourMinuteSecond,0,wide);
    CHECK(f.minute.x==f.colon1X+26 && f.secondX-f.colon2X==26 && insideCircle(v,f.hour) && insideCircle(v,f.second));
}
// An app's icon fitted into a chip: scaled with its aspect, centred, averaged.
void maskFitting() {
    // 44x44 fully covered, into 36x36: all of it covered.
    std::array<uint8_t,44*44> full{}; full.fill(255);
    std::array<uint8_t,36*36> out{};
    CHECK(fitMask({full.data(),44,44},36,36,out.data()));
    for (auto a:out) CHECK(a==255);
    // A 4x2 mask into 8x8 keeps its aspect: 8x4 in the middle rows.
    const uint8_t wide[8]={255,255,255,255,255,255,255,255};
    std::array<uint8_t,64> box{}; box.fill(7);
    CHECK(fitMask({wide,4,2},8,8,box.data()));
    for (int y=0;y<8;++y) for (int x=0;x<8;++x) CHECK(box[y*8+x]==(y>=2 && y<6 ? 255 : 0));
    // Halving averages: a checkerboard becomes an even grey.
    std::array<uint8_t,16> checker{};
    for (int i=0;i<16;++i) checker[i]=((i%4)+(i/4))%2 ? 255 : 0;
    std::array<uint8_t,4> half{};
    CHECK(fitMask({checker.data(),4,4},2,2,half.data()));
    for (auto a:half) CHECK(a==128);
    // Nothing usable, nothing written.
    std::array<uint8_t,4> untouched{{9,9,9,9}};
    CHECK(!fitMask({nullptr,4,4},2,2,untouched.data()) && !fitMask({full.data(),0,4},2,2,untouched.data()));
    CHECK(!fitMask({full.data(),4,4},0,2,untouched.data()) && untouched[0]==9);
    // Blending in RGB565: the ends are exact, the middle rounds per channel.
    CHECK(blend565(0xffff,0x0000,255)==0xffff && blend565(0xffff,0x0000,0)==0x0000);
    CHECK(blend565(0xffff,0x0000,128)==((16<<11)|(32<<5)|16));
    CHECK(blend565(0x349f,0x349f,77)==0x349f);
}
// The committed time digits, read the way the faces read them: their groups'
// widest values are the fixed widths docs/task10/fonts.md records, and the
// Digital layout's defaults.
std::vector<uint8_t> asset(const char* name) {
    std::ifstream in(std::string("src/ui/graphics/fonts/")+name,std::ios::binary);
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
void timeGlyphs() {
    struct Expect { const char* name; int ascent,descent,hour,minute,colon; };
    const DigitalMetrics digital;
    for (const auto& e:{Expect{"DDinProExpSemiBold100.vlw",71,1,digital.hourWidth,digital.minuteWidth,digital.colonWidth},
                        Expect{"DDinProCondensedSemiBold120.vlw",84,1,99,100,21}}) {
        const auto bytes=asset(e.name);
        VlwGlyphs glyphs;
        CHECK(glyphs.load(bytes.data(),bytes.size()));
        CHECK(glyphs.ascent()==e.ascent && glyphs.descent()==e.descent);
        int hour=glyphs.width("--"),minute=hour;
        for (int i=0;i<60;++i) {
            char two[3]={char('0'+i/10),char('0'+i%10),0};
            if (i<24) hour=std::max(hour,glyphs.width(two));
            minute=std::max(minute,glyphs.width(two));
        }
        CHECK(hour==e.hour && minute==e.minute && glyphs.width(":")==e.colon);
        // Every glyph's ink stays inside its advance, so the groups can tile.
        for (const char* c="0123456789:-";*c;++c) {
            const auto* g=glyphs.find(uint8_t(*c));
            CHECK(g && g->dx>=0 && g->dx+g->width<=g->advance && g->bitmap>=bytes.data());
            CHECK(g->bitmap+g->width*g->height<=bytes.data()+bytes.size());
        }
        CHECK(!glyphs.find('A') && glyphs.width("1A1")==2*glyphs.find('1')->advance);
        // The raised colon still fits in the time's box.
        CHECK(glyphs.find(':')->dy+digital.colonLift<=e.ascent);
        // A cut asset loads nothing.
        VlwGlyphs cut;
        CHECK(!cut.load(bytes.data(),bytes.size()-1) && !cut.loaded() && !cut.find('0'));
    }
    const auto first=asset("DDinProExpSemiBold100.vlw");
    VlwGlyphs zero; const auto* g=(zero.load(first.data(),first.size()),zero.find('0'));
    CHECK(g && g->advance==56 && g->dx==6 && g->width==45 && g->dy==71 && g->height==72);
    // The 95 character text fonts are too many for this reader, by design.
    const auto text=asset("DDinProExpBold28.vlw");
    VlwGlyphs many; CHECK(!many.load(text.data(),text.size()));
}
void watchChangeBits() {
    WatchData a,b;
    CHECK(watchChanges(a,b)==0);
    b.timeValid=true; CHECK(watchChanges(a,b)==WatchTime);
    a=b; b.localTime.tm_sec=1; CHECK(watchChanges(a,b)==WatchTime);
    a=b; b.subsecondUs=5; CHECK(watchChanges(a,b)==0);          // not a reading of its own
    a=b; b.batteryPercent=80; CHECK(watchChanges(a,b)==WatchBattery);
    a=b; b.charging=true; CHECK(watchChanges(a,b)==WatchBattery);
    a=b; b.background.count=1; b.background.items[0].label[0]='x';
    CHECK(watchChanges(a,b)==WatchBackground);
    a=b; b.background.items[0].nextChangeAt=99;                 // a deadline alone
    CHECK(watchChanges(a,b)==0);
    a=b; b.background.items[0].label[0]='y'; CHECK(watchChanges(a,b)==WatchBackground);
    a=b; b.background.items[0].appId=LaunchTargetId::External1; CHECK(watchChanges(a,b)==WatchBackground);
    static const uint8_t mask[1]={255}; static const IconBitmap icon{mask,1,1};
    a=b; b.background.items[0].icon=&icon; CHECK(watchChanges(a,b)==WatchBackground);
    a=b; b.background.items[0].suggestedColor=uint16_t(0); CHECK(watchChanges(a,b)==WatchBackground);
    a=b; b.background.items[0].suggestedColor.reset(); CHECK(watchChanges(a,b)==WatchBackground);
    a=b; b.background.count=0; b.localTime.tm_min=3;
    CHECK(watchChanges(a,b)==(WatchBackground|WatchTime));
}
int main() {
    navigation(); flick(); launcherList(); launcherController(); composition();
    listLayout(); listController(); deadlines(); repaint(); damage();
    homeInput(); digitalControl(); digitalLayoutRules(); maskFitting(); timeGlyphs(); watchChangeBits();
    std::cout<<"PASS: navigation/geometry, launcher controller, frame composition, "
               "shared list layout/controller, display deadlines, "
               "3000 differential framebuffer cases over a changing background, damage rectangle, "
               "home input, digital control, digital layout, mask fitting, time glyphs, watch changes\n";
}
