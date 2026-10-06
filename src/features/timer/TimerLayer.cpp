#include "TimerLayer.h"
#include "assets/AppIcons.h"
#include "i18n/Strings.h"
#include "ui/graphics/WatchFonts.h"
#include <algorithm>
#include <cstdio>
namespace launcher {
namespace {
// docs/Images/Timer in RGB565. The keys are white at 20% over black, SET the
// light grey at 20%, the focus the timer's lime.
constexpr uint16_t Ink=0x0000,White=0xffff,Lime=0xc789; // #C4F24F
constexpr uint16_t KeyFill=0x3186,KeyIdleFill=0x18c3,KeyIdleInk=0x6b4d;
constexpr uint16_t SetFill=0x2104,ButtonFill=0x5acb,ButtonGlow=0x9cf3;
}
void TimerLayer::build() {
    const Viewport& m=viewport_;
    const TimerModel& t=model_;
    count_=0;
    auto add=[&](Kind kind,Rect box) -> Item& {
        Item& item=items_[count_++];
        item=Item{}; item.kind=kind; item.box=box; item.fill=Ink; item.ink=White;
        return item;
    };
    switch (t.view) {
    case TimerView::Setup: {
        add(Icon,timerSetupIconBox(m));
        for (int i=0;i<TimerFieldCount;++i) {
            Item& field=add(Field,timerFieldBox(m,i));
            const bool focused=t.focus==i;
            field.fill=focused ? Lime : Ink; field.ink=focused ? Ink : White;
            std::snprintf(field.text,sizeof(field.text),"%02d",t.fields[i]%100);
        }
        for (int i=0;i<2;++i) std::snprintf(add(Colon,timerColonBox(m,i)).text,sizeof(Item::text),":");
        // With SET focused the keys type nothing, and look it.
        const bool typing=t.focus<TimerFieldCount;
        for (int d=1;d<=10;++d) {
            const int digit=d%10;
            Item& key=add(Key,timerKeyBox(m,digit));
            key.fill=typing ? KeyFill : KeyIdleFill; key.ink=typing ? White : KeyIdleInk;
            std::snprintf(key.text,sizeof(key.text),"%d",digit);
        }
        Item& set=add(Button,timerSetBox(m));
        const bool focused=t.focus==TimerFocusSet;
        set.fill=focused ? Lime : SetFill; set.ink=focused ? Ink : White;
        std::snprintf(set.text,sizeof(set.text),"SET");
        break;
    }
    case TimerView::Countdown: {
        add(Icon,timerSetupIconBox(m));
        Item& reset=add(Button,timerCountdownButtonBox(m,0));
        reset.fill=ButtonFill; reset.glow=ButtonGlow; reset.progress=t.resetFill;
        std::snprintf(reset.text,sizeof(reset.text),"RESET");
        Item& pause=add(Button,timerCountdownButtonBox(m,1));
        pause.fill=ButtonFill;
        std::snprintf(pause.text,sizeof(pause.text),"%s",t.paused ? "RESUME" : "PAUSE");
        formatTimer(t.seconds,add(Time,timerCountdownTimeBox(m)).text,sizeof(Item::text));
        break;
    }
    case TimerView::Ringing: {
        add(Icon,timerRingingIconBox(m));
        formatTimer(t.seconds,add(Time,timerRingingTimeBox(m)).text,sizeof(Item::text));
        Item& dismiss=add(Dismiss,timerDismissBox(m));
        dismiss.fill=Lime; dismiss.ink=Ink;
        std::snprintf(dismiss.text,sizeof(dismiss.text),"%s",text::Dismiss);
        break;
    }
    }
}
void TimerLayer::plan(FramePlan& frame,Gfx&) {
    const int shown=visible_ ? int(model_.view) : -1;
    if (shown!=shown_) {
        // Another view's elements take over these pixels and none knows what
        // the other painted: repaint all, and compare against nothing older.
        frame.forceFull();
        for (auto& element:elements_) element=Element{};
        shown_=shown;
    }
    count_=0;
    // Closed: register nothing, so the frame keeps its capacity free.
    if (!visible_) return;
    build();
    for (int i=0;i<count_;++i) {
        const auto& item=items_[i];
        uint32_t hash=hashString(item.text,hashValue(uint32_t(item.kind),0x9e3779b9u));
        hash=hashValue(uint32_t(item.fill)|(uint32_t(item.ink)<<16),hash);
        hash=hashValue(uint32_t(item.progress),hash);
        frame.add(elements_[i],item.box,hash);
    }
}
void TimerLayer::drawDigits(Gfx& g,const char* text,const Rect& box,uint16_t ink,uint16_t ground) const {
    const int cx=box.x+box.w/2,cy=box.y+box.h/2;
    if (digits_) {
        // Centred on the digits' own height: the colon and the figures share
        // the top of a "0", so the baseline sits half of that below the middle.
        const auto* zero=digits_->find('0');
        const int top=zero ? zero->dy : digits_->ascent();
        drawGlyphs(g,*digits_,text,cx-digits_->width(text)/2,cy+top/2,ink,ground);
        return;
    }
    g.setFont(&fonts::FreeSansBold24pt7b);
    g.setTextSize(float(box.h)/48);
    g.setTextDatum(middle_center); g.setTextColor(ink,ground);
    g.drawString(text,cx,cy);
    g.setTextSize(1);
}
void TimerLayer::drawLabel(Gfx& g,const lgfx::IFont* font,const char* text,const Rect& box,uint16_t ink,uint16_t ground,
                           bool blend) const {
    const float scale=float(std::min(viewport_.width,viewport_.height))/468;
    g.setFont(font ? font : &fonts::FreeSansBold12pt7b); g.setTextSize(scale);
    g.setTextDatum(middle_center);
    if (blend) g.setTextColor(ink); else g.setTextColor(ink,ground);
    g.drawString(text,box.x+box.w/2,box.y+box.h/2);
    g.setTextSize(1);
}
void TimerLayer::paint(Gfx& g,const PaintContext& context) {
    if (!visible_) return;
    const Viewport& m=viewport_;
    const int corner=timerCorner(m);
    for (int i=0;i<count_;++i) {
        const auto& item=items_[i];
        const auto& b=item.box;
        if (!context.clip(g,b)) continue;
        // Every element owns its whole box, ground included.
        g.fillRect(b.x,b.y,b.w,b.h,Ink);
        switch (item.kind) {
        case Icon:
            if (const auto* icon=appIcon(IconId::Timer))
                g.pushGrayscaleImage(b.x+(b.w-icon->width)/2,b.y+(b.h-icon->height)/2,icon->width,icon->height,
                                     icon->pixels,lgfx::grayscale_8bit,White,Ink);
            break;
        case Field:
            if (item.fill!=Ink) g.fillSmoothRoundRect(b.x,b.y,b.w,b.h,corner,item.fill);
            drawDigits(g,item.text,b,item.ink,item.fill);
            break;
        case Colon:
            drawDigits(g,item.text,b,item.ink,Ink);
            break;
        case Key:
            g.fillSmoothRoundRect(b.x,b.y,b.w,b.h,corner,item.fill);
            drawLabel(g,text_,item.text,b,item.ink,item.fill);
            break;
        case Button: {
            // SET is a rounded box; the countdown's buttons are pills. RESET
            // fills from the left while held, so the wearer sees when letting
            // go will reset.
            const int radius=b.h>=offsetPx(m,60) ? b.h/2 : corner;
            g.fillSmoothRoundRect(b.x,b.y,b.w,b.h,radius,item.fill);
            const int filled=b.w*item.progress/1000;
            if (filled>0 && context.clip(g,{b.x,b.y,filled,b.h})) {
                g.fillSmoothRoundRect(b.x,b.y,b.w,b.h,radius,item.glow);
                context.clip(g,b);
            }
            // Over two colours the label blends with what lies under it.
            const bool split=item.progress>0 && item.progress<1000;
            drawLabel(g,text_,item.text,b,item.ink,item.progress>=1000 ? item.glow : item.fill,split);
            break;
        }
        case Time:
            drawDigits(g,item.text,b,item.ink,Ink);
            break;
        case Dismiss:
            g.fillSmoothRoundRect(b.x,b.y,b.w,b.h,b.h/2,item.fill);
            drawLabel(g,names_,item.text,b,item.ink,item.fill);
            break;
        }
    }
}
}
