#pragma once
#include "ui/rendering/PaintContext.h"
#include "ui/graphics/Gfx.h"
#include "ui/list/ListLayout.h"
#include "ui/list/ListModel.h"
#include <array>
#include <cstddef>
namespace launcher {
// Draws one list through the frame plan: which rows are on screen, their
// boxes and fingerprints, the shortened names, and the painting itself. Each
// list owns its own view, so two lists never share history or caches.
//
// Rows are kept in a fixed number of display slots, row i in slot
// i % ListVisibleSlots, so a row keeps its slot for as long as it is visible
// and a slot handed to another row still erases the box it last painted.
// When more rows are visible than there are slots, nothing is dropped: the
// frame becomes a full repaint and every visible row is drawn directly.
//
// Two caches per slot, both keyed by content and never by pointer:
//  - the shortened name, keyed by row id, source text, font, scale and width,
//    so moving a row never shortens its name again;
//  - the name rendered once into an RGB565 sprite in PSRAM, keyed by the
//    shortened text, colour, background, font and scale, and pushed instead of drawing
//    the glyphs every frame. A failed allocation falls back to drawing the
//    text directly and is not retried until the key changes.
// Both live as long as the view and only ever occupy the slots, so they are
// bounded by ListVisibleSlots; a hidden list keeps them for its return.
//
// The view draws no background. Its owner restores it (black is the
// Renderer's base) and says which colour it is, so the names are drawn and
// cached against it; the circles blend their edges with what is already on
// the panel.
class ListView {
public:
    // Shared assets are given once, not looked up per frame.
    void begin(const lgfx::IFont* font) { font_=font; }
    // `rows` is borrowed until paint() returns. A hidden list registers its
    // slots empty, so whatever it painted last is erased. `background` is
    // the colour the owner lays under the rows (RGB565).
    void plan(FramePlan& frame,Gfx& g,const ListPlacement& placement,ListRows rows,
              const ListState& state,bool visible,uint16_t background=0);
    void paint(Gfx& g,const PaintContext& context);
    // Forgets what every slot last painted, for an owner that stops planning
    // this view for a while (another set of elements takes over its pixels)
    // and repaints in full when it returns. Until the next plan() the view
    // paints nothing. The caches stay: they are keyed by content.
    void invalidate();
    // Frees every text image and forgets every shortened name. The next frame
    // rebuilds what it shows.
    void releaseCache();
    struct CacheStats {
        uint32_t fits=0,renders=0,allocations=0,failures=0;
        size_t bytes=0;
    };
    const CacheStats& cacheStats() const { return stats_; }
    // Around the text of the image, so glyphs that overhang their advance or
    // the line height are captured whole. Kept below the 14px gap between a
    // circle and its name.
    static constexpr int ImagePadX=4,ImagePadY=6;
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    // Fewer slots, so the render check reaches reuse and the direct fallback
    // with only the launcher's five rows.
    void slotLimitForTest(int slots) { slotLimit_=std::clamp(slots,1,ListVisibleSlots); }
    // Off: names drawn as glyphs, for comparing the images against.
    void textImagesForTest(bool enabled) { imagesEnabled_=enabled; }
    void failAllocationsForTest(bool fail) { failAllocations_=fail; }
#endif
private:
    struct FittedText {
        bool valid=false;
        RowId id=0;
        const lgfx::IFont* font=nullptr;
        float scale=0;
        int width=0;
        char source[96]{},text[96]{};
    };
    struct TextImage {
        M5Canvas sprite;
        // What the sprite holds, or failed to hold; see the class comment.
        bool keyed=false,ready=false;
        char text[96]{};
        uint16_t color=0,background=0;
        const lgfx::IFont* font=nullptr;
        float scale=0;
        int anchorY=0;
    };
    struct Slot {
        Element element;
        int index=-1;
        RowLayout layout{};
        FittedText fitted;
        TextImage image;
    };
    const char* fit(Gfx& g,FittedText& fitted,const ListRow& row,int width);
    bool prepareImage(Gfx& g,TextImage& image,const char* text,uint16_t color);
    void paintRow(Gfx& g,const RowLayout& layout,const ListRow& row,bool selected,
                  const char* label,Slot* slot);
    std::array<Slot,ListVisibleSlots> slots_{};
    ListPlacement placement_{};
    ListRows rows_{};
    int selection_=-1,first_=0,last_=-1,slotLimit_=ListVisibleSlots;
    uint16_t background_=0;
    // Direct: this frame draws every visible row outside the slots. The frame
    // after one has to repaint in full too, since nothing recorded those rows.
    bool direct_=false,wasDirect_=false;
    bool imagesEnabled_=true,failAllocations_=false;
    float scale_=1;
    const lgfx::IFont* font_=nullptr;
    CacheStats stats_{};
};
}
