#include "HomeLayer.h"
#include <cstring>
namespace launcher {
bool HomeLayer::begin(Gfx& g,bool disableCache) {
    registerFace(digital_);
    return selectFace(g,digital_.id(),disableCache);
}
bool HomeLayer::registerFace(WatchFace& face) {
    for(auto* registered:registry_) if(registered && std::strcmp(registered->id(),face.id())==0) return false;
    for(auto& slot:registry_) if(!slot) { slot=&face; return true; }
    return false;
}
bool HomeLayer::selectFace(Gfx& g,const char* id,bool disableCache) {
    for(auto* candidate:registry_) if(candidate && std::strcmp(candidate->id(),id)==0) {
        WatchFace* previous=face_;
        if(previous) previous->end();
        switched_=true;
        if(!candidate->begin(g,disableCache)) {
            candidate->end();
            face_=previous && previous->begin(g,true) ? previous : nullptr;
            return false;
        }
        face_=candidate; return true;
    }
    return false;
}
void HomeLayer::plan(FramePlan& frame,Gfx& g) {
    WatchChanges changes=watchChanges(previous_,data_);
    if(progress_!=previousProgress_) changes|=WatchProgress;
    if(resumed_) changes|=WatchResumed;
    if(switched_) { frame.forceFull(); changes|=WatchSelected|WatchResumed; switched_=false; }
    resumed_=false; previous_=data_; previousProgress_=progress_;
    if(face_) {
        face_->update(data_,{region_.viewport,region_.clip,progress_},changes);
        face_->plan(frame,g,region_,data_);
    }
}
void HomeLayer::paint(Gfx& g,const FramePlan& frame) {
    if(face_) face_->paint(g,frame);
}
}
