#pragma once
#include "features/home/faces/HandsFace.h"
namespace launcher {
// Analog (docs/task11/plan.md): HandsFace on the Renderer's black, the date
// and the row in grey, an orange dot. The black is the base the Renderer
// restores, so the face has no background of its own to paint or declare.
class AnalogWatchFace final : public HandsFace {
public:
    AnalogWatchFace():HandsFace(AnalogDateX,Grey) {}
    const char* id() const override { return "analog"; }
    const char* name() const override { return "Analog"; }
    const char* storageKey() const override { return "wf_analog"; }
private:
    // RGB565 of the reference colours: the date and the row (102,102,102),
    // the dot (234,81,16).
    static constexpr uint16_t Grey=0x632c,Orange=0xea82;
    const Backdrop& backdrop() const override { return black_; }
    void paintDot(Gfx& g,const AnalogStroke& s) override;
    SolidBackdrop black_{0x0000};
};
}
