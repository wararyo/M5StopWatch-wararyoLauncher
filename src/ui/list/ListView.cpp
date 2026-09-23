#include "ListView.h"
#include "ui/Text.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
namespace launcher {
namespace {
constexpr uint16_t White=0xf7be,Lime=0xb7e0,Dimmed=0x7bcf;
template<size_t N> bool copyKey(char (&out)[N],const char* text) {
    const size_t length=std::strlen(text);
    if (length>=N) { out[0]=0; return false; }
    std::memcpy(out,text,length+1);
    return true;
}
}
const char* ListView::fit(Gfx& g,FittedText& f,const ListRow& row,int width) {
    const char* source=row.label ? row.label : "";
    if (f.valid && f.id==row.id && f.font==font_ && f.scale==scale_ && f.width==width &&
        std::strcmp(f.source,source)==0) return f.text;
    g.setFont(font_); g.setTextSize(scale_);
    fitText(g,source,f.text,sizeof(f.text),width);
    ++stats_.fits;
    // A source too long to keep as its own key is simply shortened again
    // next frame; it is never matched by a prefix or by its address.
    f.valid=copyKey(f.source,source);
    f.id=row.id; f.font=font_; f.scale=scale_; f.width=width;
    return f.text;
}
void ListView::plan(FramePlan& frame,Gfx& g,const ListPlacement& placement,ListRows rows,
                    const ListState& state,bool visible) {
    placement_=placement; rows_=rows; selection_=state.selection;
    const Viewport& m=placement.region.viewport;
    scale_=float(std::min(m.width,m.height))/468;
    const bool any=visible && visibleListRows(placement,rows.count,first_,last_);
    if (!any) { first_=0; last_=-1; }
    direct_=any && last_-first_+1>slotLimit_;
    if (direct_ || wasDirect_) frame.forceFull();
    wasDirect_=direct_;
    for (int s=0;s<ListVisibleSlots;++s) {
        auto& slot=slots_[s];
        slot.index=-1;
        if (any && !direct_ && s<slotLimit_) {
            // The one visible row that maps to this slot, if any.
            const int i=first_+((s-first_%slotLimit_)+slotLimit_)%slotLimit_;
            if (i<=last_) slot.index=i;
        }
        // An unused slot is registered empty, so the box it painted last - a
        // row that scrolled away, or the whole list going hidden - is erased.
        if (slot.index<0) { slot.handle=frame.add(slot.element,{},0); continue; }
        const auto& row=rows[slot.index];
        slot.layout=layoutListRow(placement,slot.index,row.icon);
        // A width that does not depend on the row's position, so a name is
        // shortened only when it cannot fit the screen at all.
        const char* label=fit(g,slot.fitted,row,labelWidth(m,row.icon));
        uint32_t hash=hashValue(row.id,0x9e3779b9u);
        hash=hashValue(uint32_t(slot.index==selection_)|(uint32_t(row.dimmed)<<1)|(uint32_t(row.icon)<<2),hash);
        hash=hashValue(row.iconColor,hash);
        hash=hashValue(uint32_t(reinterpret_cast<uintptr_t>(row.mask)),hash);
        hash=hashString(label,hash);
        hash=hashValue(uint32_t(slot.layout.centerY),hashValue(uint32_t(slot.layout.iconX),hash));
        hash=hashValue(uint32_t(slot.layout.labelX),hash);
        slot.handle=frame.add(slot.element,slot.layout.box,hash);
    }
    g.setTextSize(1);
}
bool ListView::prepareImage(Gfx& g,TextImage& im,const char* text,uint16_t color) {
    if (im.keyed && im.font==font_ && im.scale==scale_ && im.color==color &&
        std::strcmp(im.text,text)==0) return im.ready;
    // Keyed before allocating: a key that fails stays failed, and is not
    // allocated again every frame, until the text or its look changes.
    im.keyed=copyKey(im.text,text); im.ready=false;
    im.font=font_; im.scale=scale_; im.color=color;
    if (!im.keyed) return false;
    // The height drawString centres on, computed as it does: from the font's
    // line height at size 1 and the 16.16 fixed point scale.
    g.setFont(font_); g.setTextSize(1);
    const int32_t line=g.fontHeight();
    g.setTextSize(scale_);
    const int32_t height=(line*int32_t(65536*scale_))>>16;
    const int w=int(g.textWidth(text))+2*ImagePadX,h=height+2*ImagePadY;
    if (im.sprite.width()!=w || im.sprite.height()!=h) {
        if (im.sprite.getBuffer()) stats_.bytes-=size_t(im.sprite.width())*im.sprite.height()*2;
        im.sprite.deleteSprite();
        im.sprite.setPsram(true); im.sprite.setColorDepth(16);
        if (failAllocations_ || !im.sprite.createSprite(w,h)) {
            im.sprite.deleteSprite();
            ++stats_.failures;
            std::printf("[ListView] text image allocation failed: %dx%d, drawing directly\n",w,h);
            return false;
        }
        ++stats_.allocations; stats_.bytes+=size_t(w)*h*2;
    }
    // Drawn exactly as the direct path draws it: same font, scale, datum and
    // colours over black, so the push matches pixel for pixel. drawString's
    // placement only ever subtracts offsets from the point it is given, so
    // drawing it here at (pad, anchor) is the same picture moved.
    im.anchorY=ImagePadY+(height>>1);
    im.sprite.fillScreen(0);
    im.sprite.setFont(font_); im.sprite.setTextSize(scale_);
    im.sprite.setTextDatum(middle_left); im.sprite.setTextColor(color,0);
    im.sprite.drawString(text,ImagePadX,im.anchorY);
    ++stats_.renders;
    im.ready=true;
    return true;
}
void ListView::releaseCache() {
    for (auto& slot:slots_) {
        // Field by field: the sprite owns its buffer and is not assignable.
        auto& im=slot.image;
        im.sprite.deleteSprite();
        im.keyed=im.ready=false; im.text[0]=0; im.font=nullptr; im.scale=0; im.color=0;
        slot.fitted=FittedText{};
    }
    stats_.bytes=0;
}
void ListView::paintRow(Gfx& g,const RowLayout& r,const ListRow& row,bool selected,
                        const char* label,Slot* slot) {
    const auto& b=r.box;
    const Viewport& m=placement_.region.viewport;
    g.setClipRect(b.x,b.y,b.w,b.h);
    if (row.icon) {
        const int radius=r.radius-(selected ? 0 : selectionGrowth(m));
        // An even diameter, so the circle centres on the pixel boundary at
        // (iconX, centerY) where the 44px mask and the even height text box
        // centre too. fillCircle would cover 2r+1 and land half a pixel off.
        g.fillSmoothRoundRect(r.iconX-radius,r.centerY-radius,2*radius,2*radius,radius,row.iconColor);
        // The mask fits inside the circle, so its zero pixels restore the
        // circle colour and no pixel outside the circle is touched.
        if (const auto* icon=row.mask) {
            if (scale_==1.0f)
                g.pushGrayscaleImage(r.iconX-icon->width/2,r.centerY-icon->height/2,
                    icon->width,icon->height,icon->pixels,lgfx::grayscale_8bit,White,row.iconColor);
            else
                // Rotate-zoom takes pixel indices and adds half a pixel to each,
                // so both centres are given as index-0.5 to stay on the boundary.
                g.pushGrayscaleImageRotateZoom(r.iconX-0.5f,r.centerY-0.5f,
                    icon->width*0.5f-0.5f,icon->height*0.5f-0.5f,0.0f,scale_,scale_,
                    icon->width,icon->height,icon->pixels,lgfx::grayscale_8bit,White,row.iconColor);
        }
    }
    // A dimmed row greys its name out. The icon is left alone so the rows
    // still scan as one column.
    const uint16_t color=selected ? Lime : (row.dimmed ? Dimmed : White);
    // The image's padding is black, which is what lies around the name: the
    // row was erased and the circle ends well left of the text.
    if (slot && imagesEnabled_ && label[0] && prepareImage(g,slot->image,label,color)) {
        slot->image.sprite.pushSprite(&g,r.labelX-ImagePadX,r.centerY-slot->image.anchorY);
        return;
    }
    g.setFont(font_); g.setTextSize(scale_);
    g.setTextDatum(middle_left);
    g.setTextColor(color,0);
    g.drawString(label,r.labelX,r.centerY);
}
void ListView::paint(Gfx& g,const FramePlan& frame) {
    if (direct_) {
        // The frame is a full repaint (plan forced it), so every visible row
        // is drawn here without a slot or a cache.
        const Viewport& m=placement_.region.viewport;
        char label[96];
        for (int i=first_;i<=last_;++i) {
            const auto& row=rows_[i];
            const auto layout=layoutListRow(placement_,i,row.icon);
            if (layout.box.empty()) continue;
            g.setFont(font_); g.setTextSize(scale_);
            fitText(g,row.label ? row.label : "",label,sizeof(label),labelWidth(m,row.icon));
            paintRow(g,layout,row,i==selection_,label,nullptr);
        }
    } else {
        for (auto& slot:slots_) {
            if (slot.index<0 || slot.layout.box.empty() || !frame.shouldPaint(slot.handle)) continue;
            paintRow(g,slot.layout,rows_[slot.index],slot.index==selection_,slot.fitted.text,&slot);
        }
    }
    g.clearClipRect(); g.setTextSize(1);
}
}
