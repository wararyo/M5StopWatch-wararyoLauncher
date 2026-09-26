#pragma once
#include "ui/rendering/Element.h"
#include "ui/rendering/Viewport.h"
#include "ui/graphics/Gfx.h"
#include "ui/rendering/Renderer.h"
namespace launcher {
// The short notice over the lower part of the screen. Above every screen and
// below the statistics chip. How long a notice stays is its sender's business
// (ScreenManager); this only places, shortens and paints it.
//
// A notice appearing, changing or going away is an ordinary change of its
// element: the frame restores its box and whatever lies under it repaints
// there, backgrounds included.
class ToastLayer final : public RenderLayer {
public:
    void begin(const lgfx::IFont* font) { font_=font; }
    // `text` is borrowed until paint() returns; nullptr shows no notice.
    void prepare(Viewport viewport,const char* text) { viewport_=viewport; text_=text; }
    void plan(FramePlan& frame,Gfx& g) override;
    void paint(Gfx& g,const PaintContext& context) override;
private:
    const lgfx::IFont* font_=nullptr;
    Viewport viewport_{};
    const char* text_=nullptr;
    // Keyed by content: a pointer would call two equal notices different,
    // and one reused buffer the same.
    Element element_;
    Rect box_{};
    char fitted_[96]{};
};
}
