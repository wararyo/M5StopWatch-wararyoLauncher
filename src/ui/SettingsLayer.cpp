#include "SettingsLayer.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace launcher {
namespace {
constexpr uint16_t White=0xf7be,Muted=0xad75,Lime=0xb7e0,Panel=0x2104,Ink=0x0000;
constexpr const char* MenuNames[]={"日時","輝度","消灯時間","情報","戻る"};
constexpr const char* Titles[]={"","日時","輝度","消灯時間","情報"};
}
void SettingsLayer::build(const ScreenModel& m) {
    count_=0;
    const auto& s=m.settings;
    auto add=[&](Kind kind,Rect box,int index,bool selected) -> Item& {
        Item& item=items_[count_++];
        item=Item{}; item.kind=kind; item.box=box; item.index=index; item.selected=selected;
        return item;
    };
    if (s.view==SettingsView::Menu) {
        for (int i=0;i<SettingsMenuRows;++i) {
            const auto row=settingsMenuRow(m,i);
            Item& item=add(MenuRow,row.box,i,i==s.cursor);
            item.labelX=settingsMenuLabelX(m,row); item.centerY=row.centerY;
            // The value lives on the row so the menu answers "what is it now?"
            // without opening the editor.
            if (i==1) std::snprintf(item.text,sizeof(item.text),"%s  %d",MenuNames[i],m.brightness);
            else if (i==2) std::snprintf(item.text,sizeof(item.text),"%s  %d秒",MenuNames[i],m.screenOffSec);
            else std::snprintf(item.text,sizeof(item.text),"%s",MenuNames[i]);
        }
        return;
    }
    Item& title=add(Title,settingsTitleBox(m),0,false);
    std::snprintf(title.text,sizeof(title.text),"%s",Titles[int(s.view)]);
    const int fields=settingsFieldCount(s.view);
    for (int i=0;i<fields;++i) {
        Item& item=add(Field,settingsFieldBox(m,i),i,i==s.cursor);
        item.editing=item.selected && s.editing;
        if (s.view==SettingsView::DateTime)
            std::snprintf(item.text,sizeof(item.text),i==0 ? "%04d" : "%02d",s.fields[i]);
        else if (s.view==SettingsView::Brightness)
            std::snprintf(item.text,sizeof(item.text),"%d",s.fields[i]);
        else
            std::snprintf(item.text,sizeof(item.text),"%d秒",int(ScreenOffChoices[s.fields[i]]));
    }
    for (int i=0;i<settingsSeparatorCount(s.view);++i) {
        Item& item=add(Separator,settingsSeparatorBox(m,i),i,false);
        std::snprintf(item.text,sizeof(item.text),"%s",i==2 ? ":" : "/");
    }
    if (s.view==SettingsView::Info)
        for (int i=0;i<3;++i) {
            Item& item=add(InfoLine,settingsInfoBox(m,i),i,false);
            std::snprintf(item.text,sizeof(item.text),"%s",s.lines[i] ? s.lines[i] : "-");
        }
    for (int i=0;i<settingsButtonCount(s.view);++i) {
        Item& item=add(Button,settingsButtonBox(m,i),i,s.cursor==fields+i);
        const char* label=s.view==SettingsView::Info ? "戻る" : i==0 ? "保存" : "キャンセル";
        std::snprintf(item.text,sizeof(item.text),"%s",label);
    }
}
void SettingsLayer::plan(FramePlan& frame,Gfx& g,const ScreenModel& m,const lgfx::IFont* font) {
    (void)g; (void)font;
    count_=0;
    // Closed: register nothing. The screen change already forces a full repaint,
    // so there is no leftover to erase and the frame keeps its capacity free.
    if (m.screen!=ScreenId::Settings) return;
    build(m);
    for (int i=0;i<Capacity;++i) {
        const bool used=i<count_;
        // Unused slots are registered empty so a view with fewer elements
        // still erases what the previous one drew.
        uint32_t hash=0;
        if (used) {
            const auto& item=items_[i];
            hash=hashString(item.text,hashValue(uint32_t(item.kind),0x9e3779b9u));
            hash=hashValue(uint32_t(item.selected)|(uint32_t(item.editing)<<1),hash);
            hash=hashValue(uint32_t(item.centerY),hashValue(uint32_t(item.labelX),hash));
        }
        handles_[i]=frame.add(elements_[i],used ? items_[i].box : Rect{},hash);
    }
}
void SettingsLayer::paint(Gfx& g,const FramePlan& frame,const ScreenModel& m,const lgfx::IFont* font) {
    if (m.screen!=ScreenId::Settings) return;
    const float scale=float(std::min(m.width,m.height))/468;
    const int arrowW=offsetPx(m,22),arrowH=offsetPx(m,12);
    for (int i=0;i<count_;++i) {
        const auto& item=items_[i];
        if (item.box.empty() || !frame.shouldPaint(handles_[i])) continue;
        g.setClipRect(item.box.x,item.box.y,item.box.w,item.box.h);
        g.setFont(font); g.setTextSize(scale);
        switch (item.kind) {
        case Title:
            g.setTextDatum(middle_center); g.setTextColor(Muted,Ink);
            g.drawString(item.text,item.box.x+item.box.w/2,item.box.y+item.box.h/2);
            break;
        case MenuRow:
            g.setTextDatum(middle_left); g.setTextColor(item.selected ? Lime : White,Ink);
            g.drawString(item.text,item.labelX,item.centerY);
            break;
        case Field: {
            const uint16_t colour=item.selected ? Lime : White;
            const int cx=item.box.x+item.box.w/2;
            const int top=item.box.y+offsetPx(m,9),bottom=item.box.y+item.box.h-offsetPx(m,9);
            // Plain triangles: the arrows are affordances, not artwork, and
            // this keeps them independent of the embedded glyph subset.
            g.fillTriangle(cx,top,cx-arrowW/2,top+arrowH,cx+arrowW/2,top+arrowH,
                           item.selected ? Lime : Muted);
            g.fillTriangle(cx,bottom,cx-arrowW/2,bottom-arrowH,cx+arrowW/2,bottom-arrowH,
                           item.selected ? Lime : Muted);
            const Rect value=settingsValueBox(m,item.index);
            if (item.editing) {
                g.fillRoundRect(value.x,value.y,value.w,value.h,offsetPx(m,10),Panel);
                g.setTextColor(Lime,Panel);
            } else g.setTextColor(colour,Ink);
            g.setTextDatum(middle_center);
            g.drawString(item.text,value.x+value.w/2,value.y+value.h/2);
            break;
        }
        case Separator:
            g.setTextDatum(middle_center); g.setTextColor(Muted,Ink);
            g.drawString(item.text,item.box.x+item.box.w/2,item.box.y+item.box.h/2);
            break;
        case InfoLine:
            g.setTextDatum(middle_center); g.setTextColor(White,Ink);
            g.drawString(item.text,item.box.x+item.box.w/2,item.box.y+item.box.h/2);
            break;
        case Button: {
            const uint16_t background=item.selected ? Lime : Panel;
            g.fillRoundRect(item.box.x,item.box.y,item.box.w,item.box.h,offsetPx(m,14),background);
            g.setTextDatum(middle_center); g.setTextColor(item.selected ? Ink : White,background);
            g.drawString(item.text,item.box.x+item.box.w/2,item.box.y+item.box.h/2);
            break;
        }
        }
    }
    g.clearClipRect(); g.setTextSize(1);
}
}
