#include "ExternalLayer.h"
#include "ui/graphics/Text.h"
#include <algorithm>
#include <cstdio>
namespace launcher {
namespace {
constexpr uint16_t White=0xf7be,Muted=0xad75,Lime=0xb7e0,Panel=0x2104,Ink=0x0000,Alert=0xfd00;
// The state words the list deliberately does not carry: the row only dims, and
// the reason is read here (plan.md 5.2 keeps the list to icon and name).
const char* statusText(SlotStatus status) {
    switch (status) {
    case SlotStatus::Scanning: return "検証中";
    case SlotStatus::Empty: return "空き";
    case SlotStatus::Invalid: return "破損";
    case SlotStatus::ReadError: return "読み取り失敗";
    case SlotStatus::Unsupported: return "非対応";
    case SlotStatus::Ready: break;
    }
    return "";
}
}
void ExternalLayer::build(Gfx& g,const lgfx::IFont* font,Viewport m,const ExternalModel& e) {
    count_=0;
    g.setFont(font); g.setTextSize(float(std::min(m.width,m.height))/468);
    auto add=[&](Kind kind,Rect box,int index,bool selected) -> Item& {
        Item& item=items_[count_++];
        item=Item{}; item.kind=kind; item.box=box; item.index=index; item.selected=selected;
        return item;
    };
    // The name and the version come out of a guest image, so they go through
    // fitText: an uncovered glyph becomes '?' and an over-long name is cut with
    // whole characters instead of running under the bezel.
    auto text=[&](Item& item,const char* source) {
        fitText(g,source,item.text,sizeof(item.text),item.box.w);
    };
    char buffer[96];
    Item& title=add(Title,externalTitleBox(m),0,false);
    if (e.name) std::snprintf(buffer,sizeof(buffer),"%s",e.name);
    else std::snprintf(buffer,sizeof(buffer),"外部アプリ%d",e.slot);
    text(title,buffer);
    Item& slot=add(Line,externalLineBox(m,0),0,false);
    std::snprintf(buffer,sizeof(buffer),"スロット%d",e.slot);
    text(slot,buffer);
    Item& state=add(Line,externalLineBox(m,1),1,false);
    if (e.phase==ExternalPhase::BootCommitting)
        std::snprintf(buffer,sizeof(buffer),"起動中");
    else if (e.status==SlotStatus::Ready)
        std::snprintf(buffer,sizeof(buffer),"バージョン %s",e.version ? e.version : "-");
    else
        std::snprintf(buffer,sizeof(buffer),"%s",statusText(e.status));
    text(state,buffer);
    if (e.phase==ExternalPhase::BootFailed) {
        Item& failure=add(Line,externalLineBox(m,2),2,false);
        failure.alert=true;
        std::snprintf(buffer,sizeof(buffer),"起動できませんでした %s",e.message ? e.message : "");
        text(failure,buffer);
    } else if (e.phase==ExternalPhase::Browsing && e.error!=0) {
        Item& diagnostic=add(Line,externalLineBox(m,2),2,false);
        std::snprintf(buffer,sizeof(buffer),"エラー 0x%x",unsigned(e.error));
        text(diagnostic,buffer);
    }
    for (int i=0;i<externalButtonCount(e);++i) {
        Item& item=add(Button,externalButtonBox(m,i),i,e.cursor==i);
        std::snprintf(item.text,sizeof(item.text),"戻る");
    }
}
void ExternalLayer::plan(FramePlan& frame,Gfx& g) {
    count_=0;
    // Closed: register nothing. The screen change already forces a full repaint,
    // so there is no leftover to erase and the frame keeps its capacity free.
    if (!visible_) return;
    build(g,font_,viewport_,model_);
    for (int i=0;i<Capacity;++i) {
        const bool used=i<count_;
        // Unused slots are registered empty so a phase with fewer elements
        // still erases what the previous one drew.
        uint32_t hash=0;
        if (used) {
            const auto& item=items_[i];
            hash=hashString(item.text,hashValue(uint32_t(item.kind),0x9e3779b9u));
            hash=hashValue(uint32_t(item.selected)|(uint32_t(item.alert)<<1),hash);
            hash=hashValue(uint32_t(item.index),hash);
        }
        handles_[i]=frame.add(elements_[i],used ? items_[i].box : Rect{},hash);
    }
}
void ExternalLayer::paint(Gfx& g,const FramePlan& frame) {
    if (!visible_) return;
    const Viewport& m=viewport_;
    const lgfx::IFont* font=font_;
    const float scale=float(std::min(m.width,m.height))/468;
    for (int i=0;i<count_;++i) {
        const auto& item=items_[i];
        if (item.box.empty() || !frame.shouldPaint(handles_[i])) continue;
        g.setClipRect(item.box.x,item.box.y,item.box.w,item.box.h);
        g.setFont(font); g.setTextSize(scale);
        g.setTextDatum(middle_center);
        switch (item.kind) {
        case Title:
            g.setTextColor(White,Ink);
            g.drawString(item.text,item.box.x+item.box.w/2,item.box.y+item.box.h/2);
            break;
        case Line:
            g.setTextColor(item.alert ? Alert : Muted,Ink);
            g.drawString(item.text,item.box.x+item.box.w/2,item.box.y+item.box.h/2);
            break;
        case Button: {
            const uint16_t background=item.selected ? Lime : Panel;
            g.fillRoundRect(item.box.x,item.box.y,item.box.w,item.box.h,offsetPx(m,14),background);
            g.setTextColor(item.selected ? Ink : White,background);
            g.drawString(item.text,item.box.x+item.box.w/2,item.box.y+item.box.h/2);
            break;
        }
        }
    }
    g.clearClipRect(); g.setTextSize(1);
}
}
