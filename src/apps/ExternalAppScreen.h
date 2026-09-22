#pragma once
#include "apps/AppScreen.h"
#include "multifirm/SlotService.h"
#include "ui/ExternalLayout.h"
namespace launcher {
// The external app screen of plan.md 8.2. Deciding a launchable row in the list
// IS the final decision, so entering one goes straight to the commit; this
// screen only shows the launch in progress, a slot that cannot be launched, or
// a launch that failed.
class ExternalAppScreen final : public AppScreen {
public:
    void resize(int width,int height) override { width_=width; height_=height; }
    // The catalog is owned by ScreenManager and outlives this screen.
    void bind(SlotService* slots,const SlotCatalog* catalog) { slots_=slots; catalog_=catalog; }
    bool available() const override { return slots_ && catalog_; }
    void select(int slot) { slot_=slot; }
    void enter(TimeUs) override;
    void exit() override;
    ScreenOutcome handle(const Events& e,TimeUs now) override;
    ExternalModel model() const;
    bool exclusive() const override { return phase_==ExternalPhase::BootCommitting; }
    // Called once the committing frame has reached the panel. Returns true when
    // the model changed, which only happens if the API came back with a failure.
    bool commitPendingBoot();
private:
    ScreenModel layoutModel() const;
    SlotService* slots_=nullptr;
    const SlotCatalog* catalog_=nullptr;
    int slot_=1;
    ExternalPhase phase_=ExternalPhase::Browsing;
    bool issued_=false;
    const char* message_=nullptr;
    int width_=468,height_=468;
};
}
