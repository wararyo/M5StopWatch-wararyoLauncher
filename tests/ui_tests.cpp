#include "app/AppRuntime.h"
#include "ui/Element.h"
#include "ui/ListLayout.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <random>
#define CHECK(x) do { if(!(x)) { std::cerr<<__LINE__<<": " #x "\n"; std::exit(1); } } while(false)
using namespace launcher;
void navigation() {
    ScreenManager s; TimeUs now=0; Events e{};
    e.gesture=Gesture::Tap; e.x=234; e.y=234;
    CHECK(!s.handle(e,now) && s.model().screen==ScreenId::Home);
    auto drag=[&](int dy) {
        e={}; e.gesture=Gesture::DragStart; e.totalY=dy; s.handle(e,now);
        e.gesture=Gesture::DragEnd; s.handle(e,now);
        now+=180000; s.update(now);
    };
    drag(-50); CHECK(s.model().screen==ScreenId::Home && s.model().transition==0);
    CHECK((s.model().homeRegion.offsetY==0 &&
           s.model().homeRegion.clip==Rect{0,0,468,468}));
    drag(-51); CHECK(s.model().screen==ScreenId::AppList && s.model().transition==1);
    CHECK(s.model().homeRegion.offsetY==-468 && s.model().homeRegion.clip.empty());
    for(int i=1;i<=5;++i) {
        e={}; e.next=true; s.handle(e,now); now+=180000; s.update(now);
        CHECK(s.model().selection==i%5);
        const auto row=layoutRow(listGeometry(s.model()),i%5);
        CHECK(row.box.contains(row.labelX,row.centerY));
    }
    drag(50); CHECK(s.model().screen==ScreenId::AppList);
    drag(51); CHECK(s.model().screen==ScreenId::Home);
    e={}; e.next=true; s.handle(e,now); now+=180000; s.update(now);
    drag(-200); CHECK(s.model().scroll>0);
    // Hitting the top during a list gesture does not turn it into Home.
    drag(1000); CHECK(s.model().scroll==0 && s.model().screen==ScreenId::AppList);
    drag(100); CHECK(s.model().screen==ScreenId::Home);
    e={}; e.next=true; s.handle(e,now); now+=180000; s.update(now);
    e={}; e.gesture=Gesture::Tap; e.x=0; e.y=0;
    CHECK(!s.handle(e,now) && !s.model().toast);
    const auto row=layoutRow(listGeometry(s.model()),1);
    e.x=row.labelX; e.y=row.centerY; s.handle(e,now);
    CHECK(s.model().selection==1 && s.model().toast);
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
    ScreenManager a,b; e={}; e.next=true;
    a.handle(e,0); b.handle(e,0);
    for(int t=16000;t<=160000;t+=16000) a.update(t);
    b.update(160000); CHECK(a.model().transition==b.model().transition);
    b.update(10000000); CHECK(b.model().transition==1 && b.nextUpdate()==INT64_MAX);
    ScreenManager rapid; e={}; e.next=true;
    rapid.handle(e,0); rapid.handle(e,10000); rapid.handle(e,20000);
    CHECK(rapid.model().selection==2);
    rapid.update(200000); CHECK(rapid.model().transition==1 && rapid.model().scroll==2*rowSpacing(rapid.model().viewport()));
    for(int side: {400,466,468}) {
        ListGeometry m; m.width=m.height=side; m.transition=1;
        int previousX=side,previousDy=-side,highest=side,lowest=0;
        for(int scroll=0;scroll<=4*rowSpacing(m);scroll+=3) {
            m.scroll=scroll;
            for(int i=0;i<5;++i) {
                const auto r=layoutRow(m,i); const auto box=r.box;
                if(box.empty()) continue;
                CHECK(box.x>=0 && box.y>=0 && box.x+box.w<=side && box.y+box.h<=side);
                // The box reaches the right edge so it covers a name the bezel
                // cuts off; only the icon has to stay on the panel.
                CHECK(box.x+box.w==side && box.x<=r.iconX-r.radius);
                CHECK(r.iconX-r.radius>=0 && r.iconX+r.radius<=side);
                CHECK(hitRow(m,box.x+box.w/2,box.y+box.h/2)==i);
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
    ScreenManager slow,fast,sparse;
    release(slow,0); release(fast,1000); release(sparse,1000);
    float previous=fast.model().scroll;
    for(int t=266000;t<430000;t+=16000) {
        fast.update(t);
        CHECK(fast.model().scroll>=previous && fast.model().scroll<=168);
        previous=fast.model().scroll;
    }
    sparse.update(426000);
    CHECK(std::abs(fast.model().scroll-sparse.model().scroll)<0.001f);
    slow.update(450000); fast.update(450000);
    CHECK(slow.model().scroll==84 && fast.model().scroll==168);
    CHECK(!fast.active() && fast.nextUpdate()==INT64_MAX);
    CHECK(!fast.update(1000000));
    for(float speed: {-100000.0f,-1000.0f,0.0f,1000.0f,100000.0f}) {
        ScreenManager s; release(s,speed,-200);
        float last=s.model().scroll;
        s.update(430000); const float end=s.model().scroll;
        ScreenManager sample; release(sample,speed,-200);
        for(int t=266000;t<=442000;t+=16000) {
            sample.update(t); const float pos=sample.model().scroll;
            CHECK(pos>=0 && pos<=336);
            CHECK(end>=200 ? pos>=last && pos<=end : pos<=last && pos>=end);
            last=pos;
        }
        CHECK(sample.model().screen==ScreenId::AppList);
    }
    ScreenManager stopped; release(stopped,1000);
    Events e{}; e.gesture=Gesture::TouchStart; stopped.handle(e,282000);
    const float pos=stopped.model().scroll;
    CHECK(!stopped.active()); stopped.update(350000); CHECK(stopped.model().scroll==pos);
    e.gesture=Gesture::Tap; e.x=234; e.y=234; stopped.handle(e,360000);
    CHECK(!stopped.model().toast); stopped.update(540000);
    CHECK(!stopped.active());
    ScreenManager resumed; release(resumed,1000);
    e={}; e.gesture=Gesture::TouchStart; resumed.handle(e,282000);
    const float held=resumed.model().scroll;
    e.gesture=Gesture::DragStart; e.totalY=-15; resumed.handle(e,300000);
    CHECK(std::abs(resumed.model().scroll-held-15)<0.001f);
    e.gesture=Gesture::DragEnd; resumed.handle(e,320000); resumed.update(500000);
    CHECK(!resumed.active() && !resumed.model().toast);
    ScreenManager edge; release(edge,100000,-1000); edge.update(450000);
    CHECK(edge.model().scroll==336 && edge.model().screen==ScreenId::AppList);
    ScreenManager decide; release(decide,1000);
    e={}; e.decide=true; decide.handle(e,282000); decide.update(462000);
    CHECK(decide.model().toast && decide.model().scroll==decide.model().selection*84);
    ScreenManager buttons; release(buttons,1000);
    e={}; e.next=true; buttons.handle(e,282000); buttons.update(462000);
    CHECK(buttons.model().scroll==buttons.model().selection*84);
    e={}; e.home=true; buttons.handle(e,470000);
    CHECK(!buttons.active() && buttons.model().screen==ScreenId::Home);
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
    void draw(const ScreenModel&,const WatchData&) override { ++draws; }
    WatchData sample(TimeUs now) override {
        ++samples; WatchData d; d.timeValid=valid;
        d.localTime.tm_sec=(now/1000000)%60; d.subsecondUs=now%1000000; return d;
    }
    TimeUs nextUpdate(TimeUs now,const WatchData& d) const override { return nextMinute(now,d); }
};
void deadlines() {
    Platform p; p.valid=true;
    AppRuntime r(p,p,p,468,468); r.begin(); r.step(); CHECK(p.draws==1);
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
}
int main() {
    navigation(); flick(); deadlines(); repaint();
    std::cout<<"PASS: navigation/geometry, display deadlines, "
               "3000 differential framebuffer cases with dirty bounds\n";
}
