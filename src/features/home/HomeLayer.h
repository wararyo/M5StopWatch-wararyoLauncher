#pragma once
#include "features/home/WatchFace.h"
#include "features/home/faces/DigitalWatchFace.h"
#include "ui/rendering/Renderer.h"
#include <array>
namespace launcher {
// The clock layer and the faces it can show: which faces exist, which one is
// selected, their begin/end and cache lifetime, and the next minute (or
// whatever the face asks for) that the runtime has to wake up for.
//
// Lifetime: the selected face is ended in the destructor, before any member
// goes. The built-in face is a member; a face registered from outside must
// outlive this layer, so no face is ever left pointing at freed storage.
class HomeLayer final : public RenderLayer {
public:
    ~HomeLayer() override { if(face_) face_->end(); }
    // Registers and selects the built-in face.
    bool begin(Gfx& g,bool disableCache=false);
    // A full registry or a duplicate id is refused.
    bool registerFace(WatchFace& face);
    // A face that fails to begin is ended again and the previous one comes
    // back, uncached, so the clock never disappears. Either way the next frame
    // is a full repaint: the faces share no history.
    bool selectFace(Gfx& g,const char* id,bool disableCache=false);
    // The frame's input. An empty clip leaves the face registering empty
    // boxes, which erase whatever it painted last.
    void prepare(const DrawRegion& region,const WatchData& data,float listProgress) {
        region_=region; data_=data; progress_=listProgress;
    }
    // The next frame is a full repaint (a wake, another screen before it).
    void resume() { resumed_=true; }
    HomeOutcome handle(const HomeEvent& event) { return face_ ? face_->handle(event) : HomeOutcome{}; }
    BackgroundInterest backgroundInterest(const WatchData& data) const {
        return face_ ? face_->backgroundInterest(data.background) : BackgroundInterest{};
    }
    uint16_t listBackground() const { return face_ ? face_->listBackground() : 0; }
    const DigitalWatchFace& digital() const { return digital_; }
    void plan(FramePlan& frame,Gfx& g) override;
    void paint(Gfx& g,const FramePlan& frame) override;
    TimeUs nextUpdate(TimeUs now,const WatchData& data) const {
        return face_ ? face_->nextUpdate(now,data) : INT64_MAX;
    }
private:
    DigitalWatchFace digital_;
    std::array<WatchFace*,4> registry_{};
    WatchFace* face_=nullptr;
    bool switched_=true;
    DrawRegion region_{};
    WatchData data_{},previous_{};
    float progress_=0,previousProgress_=0;
    bool resumed_=true;
};
}
