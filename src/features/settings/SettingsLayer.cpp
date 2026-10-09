#include "SettingsLayer.h"
namespace launcher {
void SettingsLayer::plan(FramePlan& frame,Gfx& g) {
    const SettingsModel& s=model_;
    const Shown shown=!visible_ ? Shown::None : s.view==SettingsView::Menu ? Shown::Menu :
                      s.view==SettingsView::WatchFace ? Shown::Faces : Shown::Items;
    if (shown!=shown_) {
        // Another set of elements takes over these pixels, and neither the
        // list's slots nor the items know what the other painted: repaint
        // everything, and drop both histories so that nothing compares
        // against a box from before the switch.
        frame.forceFull();
        menu_.invalidate();
        editor_.invalidate();
        shown_=shown;
    }
    // Closed: register nothing, so the frame keeps its capacity free. The
    // closing frame is a full repaint (above, and the app's screen change).
    if (shown==Shown::None) return;
    if (shown==Shown::Menu) {
        const ListRows rows=buildSettingsMenuRows(rows_,&s,&labels_);
        if (labelForTest_) rows_[0].label=labelForTest_;
        menu_.plan(frame,g,settingsMenuPlacement(viewport_,s.menu.scroll),rows,s.menu,true);
        return;
    }
    if (shown==Shown::Faces) {
        // The same view as the menu, forgotten on the switch above.
        const ListRows rows=buildSettingsFaceRows(faceRows_,s.faceCount,&s,&faceLabels_);
        menu_.plan(frame,g,settingsMenuPlacement(viewport_,s.faces.scroll),rows,s.faces,true);
        return;
    }
    editor_.plan(frame,viewport_,s,stats_);
}
void SettingsLayer::paint(Gfx& g,const PaintContext& context) {
    if (!visible_ || shown_==Shown::None) return;
    if (shown_==Shown::Menu || shown_==Shown::Faces) { menu_.paint(g,context); return; }
    editor_.paint(g,context);
}
}
