#pragma once
#include "apps/AppScreen.h"
#include "services/StopwatchService.h"
#include "ui/StopwatchLayout.h"
namespace launcher {
// The stopwatch screen of plan.md 5.3. It does not follow the A-selects /
// B-confirms default: the two on-screen buttons are wired straight to A and B,
// because start, stop and lap are what gets pressed while a measurement runs
// and each of them should cost one press. There is no way back to the list;
// home returns to the clock.
//
// The screen owns no measurement state. It only samples the service, so
// leaving, going home or blanking the panel keeps the measurement running
// (plan.md 4, 5.3).
class StopwatchScreen final : public AppScreen {
public:
    void resize(int width,int height) override { width_=width; height_=height; }
    void bind(StopwatchService* stopwatch) { stopwatch_=stopwatch; }
    bool available() const override { return stopwatch_!=nullptr; }
    void enter(TimeUs now) override { sample(now); }
    // Deliberately empty beyond the deadline: the measurement is not the
    // screen's to stop (plan.md 133).
    void exit() override { next_=INT64_MAX; }
    ScreenOutcome handle(const Events& e,TimeUs now) override;
    TimeUs nextUpdate() const override { return next_; }
    bool tick(TimeUs now) override;
    const StopwatchModel& model() const { return model_; }
private:
    ScreenModel layoutModel() const;
    void sample(TimeUs now);
    // False when the press did nothing, so a dead button does not cost a
    // repaint. Only A in Reset can do that.
    bool pressLeft(TimeUs now);
    bool pressRight(TimeUs now);
    StopwatchService* stopwatch_=nullptr;
    StopwatchModel model_{};
    TimeUs next_=INT64_MAX;
    int width_=468,height_=468;
};
}
