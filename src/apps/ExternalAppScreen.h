#pragma once
#include "apps/AppScreen.h"
#include "multifirm/SlotService.h"
#include "ui/ExternalLayout.h"
namespace launcher {
// The external app detail screen of plan.md 8.2: it shows what a slot is, and
// it owns the one-way stretch between deciding to launch and the restart.
class ExternalAppScreen final : public AppScreen {
public:
    void resize(int width,int height) override { width_=width; height_=height; }
    // The catalog is owned by ScreenManager and outlives this screen.
    void bind(SlotService* slots,const SlotCatalog* catalog) { slots_=slots; catalog_=catalog; }
    bool available() const override { return slots_ && catalog_; }
    void select(int slot) { slot_=slot; }
    void enter() override;
    void exit() override;
    ScreenOutcome handle(const Events& e,TimeUs now) override;
    ExternalModel model() const;
    bool exclusive() const override { return phase_==ExternalPhase::BootCommitting; }
    // Called once the committing frame has reached the panel. Returns true when
    // the model changed, which only happens if the API came back with a failure.
    bool commitPendingBoot();
private:
    ScreenModel layoutModel() const;
    void activate(ScreenOutcome& out);
    SlotService* slots_=nullptr;
    const SlotCatalog* catalog_=nullptr;
    int slot_=1;
    ExternalPhase phase_=ExternalPhase::Browsing;
    int cursor_=0;
    bool issued_=false;
    const char* message_=nullptr;
    int width_=468,height_=468;
};
}
