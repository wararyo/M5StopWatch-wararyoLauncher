#include "ScreenManager.h"
namespace launcher {
bool DiagnosticScreen::handle(const Events& e) {
    bool changed = false;
    if (e.next) { model_.selection = (model_.selection + 1) % 2; model_.lastEvent = "Next"; changed = true; }
    if (e.decide) { model_.lastEvent = "Decide"; changed = true; }
    if (e.gesture != Gesture::None) {
        changed = true;
        switch (e.gesture) {
        case Gesture::Tap: model_.lastEvent = "Tap"; break;
        case Gesture::DragStart: model_.lastEvent = "Drag start"; model_.dragging = true; break;
        case Gesture::DragMove: model_.lastEvent = "Drag move"; break;
        case Gesture::DragEnd: model_.lastEvent = "Drag end"; model_.dragging = false; break;
        case Gesture::Cancel: model_.lastEvent = "Cancel"; model_.dragging = false; break;
        default: break;
        }
    }
    return changed;
}
bool ScreenManager::handle(const Events& e) {
    if (e.home) { ++homeCount_; switchTo(home_); return true; }
    if (current_ == &home_ && (e.next || e.decide || e.gesture == Gesture::Tap)) {
        switchTo(check_); return true;
    }
    // Touch activates the selected action; A chooses, B confirms.
    if (current_ == &check_ && (e.decide || e.gesture == Gesture::Tap) && current_->model().selection == 1) {
        switchTo(home_); return true;
    }
    return current_->handle(e);
}
}
