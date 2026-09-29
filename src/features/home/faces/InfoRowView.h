#pragma once
#include "features/home/HomeModel.h"
#include "features/home/faces/InfoRow.h"
#include "ui/graphics/Gfx.h"
#include "ui/graphics/IconBitmap.h"
#include <array>
namespace launcher {
// The information row's text and icons for Analog (and Noonish): the battery
// with its percent and up to two items, each its icon in the ink and its label,
// fitted to the row and placed by InfoRow the way Forest does. plan() decides
// what shows and where; paint() draws one part over a single background
// colour. Painting is kept apart so that a face on more than one colour can
// draw the same plan its own way.
class InfoRowView {
public:
    enum Part { Battery,Item0,Item1,PartCount };
    static constexpr int MaxItems=2;
    struct Font { const lgfx::IFont* font=nullptr; int ascent=0,descent=0; };
    // The fonts: the small D-DIN-PRO, the Japanese one for labels beyond ASCII.
    void begin(Gfx& g);
    // The icon masks are rebuilt after the next begin.
    void end();
    // This frame's row. Each part's box holds it with room for its edges, and
    // is empty when the part does not show; its key changes whenever what it
    // draws does.
    void plan(Gfx& g,const Viewport& viewport,const InfoRow& row,const WatchData& d,bool battery,int items);
    Rect box(int part) const { return boxes_[part]; }
    uint32_t key(int part) const { return keys_[part]; }
    void paint(Gfx& g,int part,uint16_t ink,uint16_t background) const;
private:
    static constexpr int MaxIcon=40;
    struct Group {
        char label[BackgroundLabelBytes+4]{};  // fitted, possibly with "..."
        bool wide=false;                        // non-ASCII: the Japanese font
        const IconBitmap* icon=nullptr;
        const IconBitmap* scaled=nullptr;       // the icon the mask was made from
        bool maskReady=false;
        uint8_t mask[MaxIcon*MaxIcon]{};
    };
    void useFont(Gfx& g,const Font& f) const;
    void paintBattery(Gfx& g,uint16_t ink,uint16_t background) const;
    void paintItem(Gfx& g,const Group& item,const Rect& box,uint16_t ink,uint16_t background) const;
    Font small_,wide_;
    Viewport viewport_{};
    InfoRow row_{};
    Group groups_[MaxItems];
    int batteryPercent_=-1;
    bool charging_=false;
    char battery_[12]{};
    std::array<Rect,PartCount> boxes_{};
    std::array<uint32_t,PartCount> keys_{};
};
}
