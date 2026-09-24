#include "ListController.h"
#include <algorithm>
#include <cmath>
namespace launcher {
int ListController::nearest() const {
    if (rows_.count<=0) return -1;
    return std::clamp(int(std::lround(scroll_/spacing())),0,rows_.count-1);
}
void ListController::select(int index) {
    selection_=index;
    hasSelectedId_=index>=0;
    if (hasSelectedId_) selectedId_=rows_[index].id;
}
void ListController::setRows(ListRows rows) {
    const int previous=selection_;
    rows_=rows.rows && rows.count>0 ? rows : ListRows{};
    if (rows_.count==0) {
        select(-1); scroll_=dragScroll_=from_=to_=0;
        dragging_=animating_=settling_=stopped_=false; nextFrame_=INT64_MAX;
        return;
    }
    int found=-1;
    if (hasSelectedId_)
        for (int i=0;i<rows_.count;++i) if (rows_[i].id==selectedId_) { found=i; break; }
    if (found<0) found=std::clamp(previous,0,rows_.count-1);
    // The row moved to another index: move the view with it, so what is on
    // screen stays put instead of jumping to a neighbour.
    if (previous>=0 && found!=previous) {
        const float shift=(found-previous)*spacing();
        scroll_+=shift; dragScroll_+=shift; from_+=shift; to_+=shift;
    }
    scroll_=std::clamp(scroll_,0.0f,maxScroll());
    to_=std::clamp(to_,0.0f,maxScroll());
    select(found);
}
void ListController::reset() {
    select(rows_.count>0 ? 0 : -1);
    scroll_=dragScroll_=from_=to_=tangent_=0;
    dragging_=animating_=settling_=stopped_=false; nextFrame_=INT64_MAX;
}
void ListController::animateTo(float scroll,TimeUs now) {
    settling_=false;
    from_=scroll_; to_=scroll; start_=now;
    animating_=from_!=to_;
    nextFrame_=animating_ ? now+FrameUs : INT64_MAX;
}
void ListController::settle(float velocity,TimeUs now) {
    const float s=spacing();
    // Strong friction: only 90 ms of look-ahead, at most 1.5 extra rows.
    const float projected=scroll_+std::clamp(velocity*0.09f,-1.5f*s,1.5f*s);
    const int target=std::clamp(int(std::lround(projected/s)),0,std::max(0,rows_.count-1));
    animateTo(target*s,now);
    settling_=true;
    const float distance=to_-from_;
    // Hermite endpoint tangents: release velocity and zero at rest. Limit the
    // initial tangent to keep the curve monotone and inside the list bounds.
    tangent_=distance==0 ? 0 : distance*std::clamp(velocity*0.18f/distance,0.0f,3.0f);
    select(nearest());
}
void ListController::align(TimeUs now) {
    animateTo(std::max(0,selection_)*spacing(),now);
}
bool ListController::next(TimeUs now) {
    if (rows_.count<=0 || dragging_) return false;
    select((selection_+1)%rows_.count);
    align(now);
    return true;
}
ListDecision ListController::decided(int index) const {
    ListDecision d;
    d.changed=true; d.index=index; d.id=rows_[index].id;
    d.decided=rows_[index].enabled;
    return d;
}
ListDecision ListController::decide(TimeUs now) {
    if (rows_.count<=0 || dragging_) return {};
    if (settling()) align(now);
    return decided(selection_);
}
ListDecision ListController::tap(const ListPlacement& placement,int x,int y,TimeUs now) {
    if (rows_.count<=0 || dragging_) return {};
    const int row=hitListRow(placement,rows_.count,x,y);
    if (row<0) return {};
    // Coasting ends on the row that was selected, and the tapped row is then
    // selected where it stands, so the animation cannot move it away.
    if (settling()) align(now);
    select(row);
    return decided(row);
}
bool ListController::touchStart() {
    stopped_=settling();
    if (stopped_) { animating_=false; nextFrame_=INT64_MAX; }
    return stopped_;
}
bool ListController::releaseAfterStop(TimeUs now) {
    if (!stopped_) return false;
    stopped_=false;
    settle(0,now);
    return true;
}
void ListController::dragStart() {
    stopped_=false;
    animating_=false; nextFrame_=INT64_MAX;
    dragScroll_=scroll_; dragging_=true;
}
bool ListController::dragMove(float totalY) {
    if (!dragging_) return false;
    scroll_=std::clamp(dragScroll_-totalY,0.0f,maxScroll());
    select(nearest());
    return true;
}
void ListController::dragEnd(float velocity,TimeUs now) {
    if (!dragging_) return;
    dragging_=false;
    settle(velocity,now);
}
void ListController::handOff() {
    stopped_=false;
    animating_=false; nextFrame_=INT64_MAX;
}
void ListController::cancel(TimeUs now) {
    stopped_=false; dragging_=false;
    align(now);
}
void ListController::finish() {
    if (animating_) {
        scroll_=to_;
        if (settling_) select(nearest());
    }
    dragging_=animating_=stopped_=false; nextFrame_=INT64_MAX;
}
bool ListController::update(TimeUs now) {
    if (!animating_ || now<nextFrame_) return false;
    const float t=std::clamp(float(now-start_)/float(AnimationUs),0.0f,1.0f);
    if (settling_) {
        const float t2=t*t,t3=t2*t;
        scroll_=from_+(to_-from_)*(3*t2-2*t3)+tangent_*(t3-2*t2+t);
        select(nearest());
    } else {
        const float eased=1-(1-t)*(1-t)*(1-t);
        scroll_=from_+(to_-from_)*eased;
    }
    animating_=t<1; nextFrame_=animating_ ? now+FrameUs : INT64_MAX;
    return true;
}
}
