#include "Element.h"
#include <algorithm>

namespace ui {

Rect clipToScreen(const Rect& r, int screenWidth, int screenHeight) {
    const int x0 = std::max<int>(r.x, 0);
    const int y0 = std::max<int>(r.y, 0);
    const int x1 = std::min<int>(r.x + r.w, screenWidth);
    const int y1 = std::min<int>(r.y + r.h, screenHeight);
    if (x1 <= x0 || y1 <= y0) return Rect{};
    return Rect{static_cast<int16_t>(x0), static_cast<int16_t>(y0),
                static_cast<int16_t>(x1 - x0), static_cast<int16_t>(y1 - y0)};
}

void FramePlan::begin(bool repaintAll) {
    count_ = 0;
    repaintAll_ = repaintAll;
    anyPaint_ = false;
}

int FramePlan::add(Element& element, const Rect& box, uint32_t fingerprint) {
    if (count_ >= Capacity) return -1;
    const int handle = count_++;
    entries_[handle] = Entry{&element, box, Rect{}, fingerprint, false};
    return handle;
}

bool FramePlan::shouldPaint(int handle) const {
    return handle >= 0 && handle < count_ && entries_[handle].paint;
}

void FramePlan::resolve(Gfx& gfx, uint16_t background) {
    for (int i = 0; i < count_; ++i) {
        Entry& e = entries_[i];
        const Element& el = *e.element;
        const bool changed = repaintAll_ || !el.valid_ || el.box_ != e.box ||
                             el.fingerprint_ != e.fingerprint;
        // After a full clear there is nothing left to erase.
        e.erase = (changed && !repaintAll_ && el.valid_) ? el.box_ : Rect{};
        e.paint = changed;
    }

    // An element that did not change still has to be painted again if somebody
    // else's erasure cuts into it. Forcing it adds no new erasure of its own,
    // since its box did not move, so one pass is enough.
    for (int i = 0; i < count_; ++i) {
        if (entries_[i].erase.empty()) continue;
        for (int j = 0; j < count_; ++j) {
            if (entries_[j].paint) continue;
            if (entries_[j].box.intersects(entries_[i].erase)) entries_[j].paint = true;
        }
    }

    for (int i = 0; i < count_; ++i) {
        const Rect& r = entries_[i].erase;
        if (!r.empty()) gfx.fillRect(r.x, r.y, r.w, r.h, background);
    }

    for (int i = 0; i < count_; ++i) {
        Entry& e = entries_[i];
        e.element->box_ = e.box;
        e.element->fingerprint_ = e.fingerprint;
        e.element->valid_ = true;
        if (e.box.empty()) e.paint = false;
        if (e.paint) anyPaint_ = true;
    }
}

}  // namespace ui
