#pragma once
#include "input/InputController.h"
namespace launcher {
enum class ScreenId { Home, InputCheck };
struct ScreenModel {
    ScreenId screen = ScreenId::Home;
    int selection = 0;
    uint32_t homeCount = 0;
    bool dragging = false;
    const char* lastEvent = "Ready";
};
// Screens only receive semantic events and produce immutable display snapshots.
class Screen {
public:
    virtual ~Screen() = default;
    virtual void enter() = 0;
    virtual void leave() = 0;
    virtual bool handle(const Events&) = 0;
    virtual ScreenModel model() const = 0;
    virtual bool active() const { return false; }
    virtual TimeUs nextUpdate() const { return INT64_MAX; }
};
class DiagnosticScreen final : public Screen {
public:
    explicit DiagnosticScreen(ScreenId id) { model_.screen = id; }
    void enter() override { model_.selection = 0; model_.dragging = false; model_.lastEvent = "Ready"; }
    void leave() override { model_.selection = 0; model_.dragging = false; }
    bool handle(const Events& e) override;
    ScreenModel model() const override { return model_; }
    bool active() const override { return model_.dragging; }
private:
    ScreenModel model_{};
};
class ScreenManager {
public:
    bool handle(const Events& e);
    ScreenModel model() const { auto m = current_->model(); m.homeCount = homeCount_; return m; }
    TimeUs nextUpdate() const { return current_->nextUpdate(); }
    bool active() const { return current_->active(); }
private:
    void switchTo(Screen& next) { current_->leave(); current_ = &next; current_->enter(); }
    DiagnosticScreen home_{ScreenId::Home}, check_{ScreenId::InputCheck};
    Screen* current_ = &home_;
    uint32_t homeCount_ = 0;
};
}
