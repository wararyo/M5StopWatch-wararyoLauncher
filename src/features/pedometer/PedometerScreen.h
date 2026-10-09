#pragma once
#include "host/Screen.h"
#include "services/PedometerService.h"
#include "features/pedometer/PedometerModel.h"
namespace launcher {
// Today's steps (docs/task13/plan.md 1.3). The count is the service's, read
// when the screen opens and every second while it is shown; the screen only
// shows it and decides when to leave. OK, A or B goes back to the list: there
// is nothing else to choose, so A, which moves the focus elsewhere, means the
// same as the button it would land on.
class PedometerScreen final : public Screen {
public:
    static constexpr TimeUs ReadUs=1000000;
    void resize(int width,int height) override { width_=width; height_=height; }
    void bind(PedometerService* service) { service_=service; }
    // Opens without an IMU too, to say there is nothing to count.
    bool available() const override { return service_!=nullptr; }
    void enter(TimeUs now) override;
    void exit() override { shown_=false; nextRead_=INT64_MAX; }
    ScreenOutcome handle(const Events& e,TimeUs now) override;
    TimeUs nextUpdate() const override { return nextRead_; }
    bool tick(TimeUs now) override;
    const PedometerModel& model() const { return model_; }
private:
    // Reads the count and re-arms the second; true when what shows moved.
    bool read(TimeUs now);
    PedometerService* service_=nullptr;
    PedometerModel model_{};
    int width_=468,height_=468;
    bool shown_=false;
    TimeUs nextRead_=INT64_MAX;
};
}
