#include "HomeLayer.h"
#include <cstdio>
namespace launcher {
bool HomeLayer::begin(Gfx& g,bool disableCache,WatchPreferences* store) {
    // Bound before any face registers: each face's record comes from it.
    selection_.bind(store && store->bound() ? store : nullptr);
    registerFace(digital_);
    registerFace(forest_);
    const int start=selection_.startup();
    if(selectFace(g,selection_.at(start).id,disableCache)) return true;
    return face_!=nullptr;
}
bool HomeLayer::registerFace(WatchFace& face) {
    auto* port=selection_.add({face.id(),face.name(),face.storageKey()});
    if(!port) { std::printf("[WatchFace] %s refused\n",face.id()); return false; }
    faces_[selection_.count()-1]=&face;
    // A face without a record, or with no storage bound, keeps its RAM state
    // only: it is given nothing to save into.
    face.bindPreferences(port->bound() ? port : nullptr);
    return true;
}
bool HomeLayer::selectFace(Gfx& g,const char* id,bool disableCache) {
    const int index=selection_.find(id);
    if(index<0) return false;
    WatchFace* candidate=faces_[index];
    WatchFace* previous=face_;
    if(previous) previous->end();
    switched_=true;
    if(candidate->begin(g,disableCache)) {
        face_=candidate; selection_.shown(index);
        return true;
    }
    candidate->end();
    std::printf("[WatchFace] %s failed to begin\n",id);
    // Back to what showed, then to Digital, both without caches.
    face_=nullptr; selection_.shown(-1);
    for(WatchFace* fallback:{previous,static_cast<WatchFace*>(&digital_)}) {
        if(!fallback || fallback==candidate) continue;
        if(fallback->begin(g,true)) { face_=fallback; selection_.shown(selection_.find(fallback->id())); break; }
        fallback->end();
    }
    return false;
}
FaceChoiceResult HomeLayer::chooseFace(Gfx& g,const char* id) {
    return selection_.choose(id,[&](int index) { return selectFace(g,selection_.at(index).id); });
}
void HomeLayer::plan(FramePlan& frame,Gfx& g) {
    WatchChanges changes=watchChanges(previous_,data_);
    if(environment_.listProgress!=previousProgress_) changes|=WatchProgress;
    if(resumed_) changes|=WatchResumed;
    if(switched_) { frame.forceFull(); changes|=WatchSelected|WatchResumed; switched_=false; }
    resumed_=false; previous_=data_; previousProgress_=environment_.listProgress;
    if(face_) {
        face_->update(data_,environment_,changes);
        face_->plan(frame,g,environment_,data_);
    }
}
void HomeLayer::paint(Gfx& g,const PaintContext& context) {
    // Whatever the face draws stays in the part the list leaves uncovered.
    if(face_) face_->paint(g,context.within(environment_.clip));
}
}
