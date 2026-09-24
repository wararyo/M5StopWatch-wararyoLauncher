#pragma once
#include "core/Time.h"
#include "ui/list/ListLayout.h"
#include "ui/list/ListModel.h"
namespace launcher {
// Selection, scrolling and inertia of one list. It knows rows by index, id and
// whether they may be decided, nothing more: what a decision opens, and any
// gesture that leaves the list (the launcher's pull back to the clock), belong
// to the screen that owns it. That screen decides at the start of a drag who
// owns it, and only a drag it hands over reaches dragStart().
// Timing is by elapsed time, never by the number of frames.
class ListController {
public:
    static constexpr TimeUs AnimationUs=180000,FrameUs=16000;
    void resize(Viewport viewport) { viewport_=viewport; }
    // Borrowed until the next call, which the owner makes whenever the rows
    // change. The selection follows its id there; if that row went, it falls
    // back into range.
    void setRows(ListRows rows);
    // The first row selected at the top, nothing moving, no deadline.
    void reset();
    // A: the next row, wrapping from the last to the first, scrolled to where
    // it is centred. Retargets from wherever an animation has got to.
    bool next(TimeUs now);
    // B: the selected row. A row that may not be decided is only selected.
    ListDecision decide(TimeUs now);
    // A tap on a row selects and decides it; a miss does nothing.
    ListDecision tap(const ListPlacement& placement,int x,int y,TimeUs now);
    // A touch landing while the list coasts stops it where it is, and the
    // release of that press (tap or drag end) then only settles: it never
    // decides the row that happened to be under the finger.
    bool touchStart();
    bool releaseAfterStop(TimeUs now);
    // Vertical drags only. `totalY` is the finger's travel since the press.
    void dragStart();
    bool dragMove(float totalY);
    // `velocity` is along the scroll: positive moves towards later rows.
    void dragEnd(float velocity,TimeUs now);
    // The owner took this press for itself: stop moving, forget the press.
    void handOff();
    // Input was cancelled: drop the drag and come to rest on the selection.
    void cancel(TimeUs now);
    // Come to rest on the selection.
    void align(TimeUs now);
    // Leaving the screen: jump to where any animation was heading and drop
    // the deadline, so a hidden list costs no frames and returns in place.
    void finish();
    // Advance an animation that is due; true when the state changed.
    bool update(TimeUs now);
    TimeUs nextUpdate() const { return nextFrame_; }
    bool active() const { return dragging_ || animating_; }
    bool settling() const { return animating_ && settling_; }
    // At the top edge, where the owner may give a downward pull its own meaning.
    bool atStart() const { return scroll_<=0.5f; }
    int count() const { return rows_.count; }
    int selection() const { return selection_; }
    float scroll() const { return scroll_; }
    ListState state() const { return {selection_,scroll_,dragging_,animating_}; }
private:
    float spacing() const { return float(rowSpacing(viewport_)); }
    float maxScroll() const { return rows_.count>1 ? (rows_.count-1)*spacing() : 0.0f; }
    int nearest() const;
    void select(int index);
    void animateTo(float scroll,TimeUs now);
    void settle(float velocity,TimeUs now);
    ListDecision decided(int index) const;
    Viewport viewport_{};
    ListRows rows_{};
    int selection_=-1;
    RowId selectedId_=0;
    bool hasSelectedId_=false;
    float scroll_=0,dragScroll_=0;
    float from_=0,to_=0,tangent_=0;
    bool dragging_=false,animating_=false,settling_=false,stopped_=false;
    TimeUs start_=0,nextFrame_=INT64_MAX;
};
}
