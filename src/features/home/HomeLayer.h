#pragma once
#include "features/home/FaceSelection.h"
#include "features/home/WatchFace.h"
#include "features/home/faces/DigitalWatchFace.h"
#include "features/home/faces/ForestWatchFace.h"
#include "ui/rendering/Renderer.h"
#include <array>
namespace launcher {
// The clock layer and the faces it can show: which faces exist, which one is
// selected and stored (FaceSelection), their begin/end and cache lifetime, and
// the next minute (or whatever the face asks for) that the runtime has to wake
// up for.
//
// Lifetime: the selected face is ended in the destructor, before any member
// goes. The built-in faces are members; a face registered from outside must
// outlive this layer, so no face is ever left pointing at freed storage.
class HomeLayer final : public RenderLayer {
public:
    ~HomeLayer() override { if(face_) face_->end(); }
    // Registers the built-in faces with their records and begins the stored
    // one, or Digital. `store` may be null (no storage: nothing is read or
    // written, every face starts from its defaults).
    bool begin(Gfx& g,bool disableCache=false,WatchPreferences* store=nullptr);
    // A full registry, a bad or duplicate id or storage key is refused. The
    // face reads its record here, once.
    bool registerFace(WatchFace& face);
    // Switches faces without storing the choice (start, diagnostics). A face
    // that fails to begin is ended again and the previous one comes back,
    // uncached, or Digital uncached if even that fails, so the clock never
    // disappears. Either way the next frame is a full repaint: the faces share
    // no history.
    bool selectFace(Gfx& g,const char* id,bool disableCache=false);
    // A choice confirmed in settings: begun, then stored
    // (docs/task10/plan-10-5.md 5).
    FaceChoiceResult chooseFace(Gfx& g,const char* id);
    int faceCount() const { return selection_.count(); }
    WatchFaceChoice faceAt(int i) const {
        return i>=0 && i<selection_.count() ? WatchFaceChoice{selection_.at(i).id,selection_.at(i).name} : WatchFaceChoice{};
    }
    int currentFace() const { return selection_.current(); }
    // The frame's input. An empty clip leaves the face registering empty
    // boxes, which erase whatever it painted last.
    void prepare(const WatchEnvironment& environment,const WatchData& data) {
        environment_=environment; data_=data;
    }
    // The next frame is a full repaint (a wake, another screen before it).
    void resume() { resumed_=true; }
    HomeOutcome handle(const HomeEvent& event) { return face_ ? face_->handle(event) : HomeOutcome{}; }
    BackgroundInterest backgroundInterest(const WatchData& data) const {
        return face_ ? face_->backgroundInterest(data.background) : BackgroundInterest{};
    }
    uint16_t listBackground() const { return face_ ? face_->listBackground() : 0; }
    const DigitalWatchFace& digital() const { return digital_; }
    const ForestWatchFace& forest() const { return forest_; }
    void plan(FramePlan& frame,Gfx& g) override;
    void paint(Gfx& g,const PaintContext& context) override;
    Rect opaqueArea() const override { return face_ ? intersect(face_->opaqueArea(),environment_.clip) : Rect{}; }
    TimeUs nextUpdate(TimeUs now,const WatchData& data) const {
        return face_ ? face_->nextUpdate(now,data) : INT64_MAX;
    }
private:
    DigitalWatchFace digital_;
    ForestWatchFace forest_;
    FaceSelection selection_;
    std::array<WatchFace*,FaceSelection::Capacity> faces_{};
    WatchFace* face_=nullptr;
    bool switched_=true;
    WatchEnvironment environment_{};
    WatchData data_{},previous_{};
    float previousProgress_=0;
    bool resumed_=true;
};
}
