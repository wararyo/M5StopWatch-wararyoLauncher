#pragma once
#include "ui/rendering/Element.h"
#include "ui/rendering/Renderer.h"
#include "ui/graphics/Gfx.h"
#include "ui/rendering/Viewport.h"
#include "features/settings/SettingsEditorView.h"
#include "features/tutorial/TutorialModel.h"
namespace launcher {
// The tutorial on a black ground: the date step is the settings date editor
// (SettingsEditorView), the way home the mark, title and lines of
// TutorialLayout.h. Only one of them is planned in a frame; going from one to
// the other keeps the screen id, so the layer itself repaints in full, as
// settings does between its menu and editors.
class TutorialLayer final : public RenderLayer {
public:
    void begin(const lgfx::IFont* font) { font_=font; editor_.begin(font); }
    void prepare(Viewport viewport,const TutorialModel& model,bool visible) {
        viewport_=viewport; model_=model; visible_=visible;
    }
    void plan(FramePlan& frame,Gfx& g) override;
    void paint(Gfx& g,const PaintContext& context) override;
private:
    enum Part : uint8_t { Mark,Title,Line,PartCount=Line+3 };
    enum class Shown : uint8_t { None,DateTime,Guide };
    Rect box(int part) const;
    const char* label(int part) const;
    Element elements_[PartCount]{};
    Shown shown_=Shown::None;
    const lgfx::IFont* font_=nullptr;
    Viewport viewport_{};
    TutorialModel model_{};
    bool visible_=false;
    SettingsEditorView editor_;
};
}
