#pragma once

#include <M5GFX.h>
#include <cstdint>

namespace ui {

/// Common base of M5GFX (the panel, which already owns a PSRAM framebuffer)
/// and M5Canvas (a sprite), so drawing code can target either.
using Gfx = m5gfx::LovyanGFX;

struct Rect {
    int16_t x = 0, y = 0, w = 0, h = 0;

    bool empty() const { return w <= 0 || h <= 0; }
    bool operator==(const Rect& o) const {
        return x == o.x && y == o.y && w == o.w && h == o.h;
    }
    bool operator!=(const Rect& o) const { return !(*this == o); }
    bool intersects(const Rect& o) const {
        return !empty() && !o.empty() && x < o.x + o.w && o.x < x + w && y < o.y + o.h &&
               o.y < y + h;
    }
};

/// Clips a box to the screen. A box entirely off screen comes back empty, which
/// is how off-screen content is culled: an empty box is never painted.
Rect clipToScreen(const Rect& r, int screenWidth, int screenHeight);

/// FNV-1a, used to tell whether an element would paint the same pixels again.
inline uint32_t hashString(const char* s, uint32_t h = 2166136261u) {
    while (*s) {
        h ^= static_cast<uint8_t>(*s++);
        h *= 16777619u;
    }
    return h;
}
inline uint32_t hashValue(uint32_t v, uint32_t h = 2166136261u) {
    for (int i = 0; i < 4; ++i) {
        h ^= (v >> (i * 8)) & 0xffu;
        h *= 16777619u;
    }
    return h;
}

/// One independently repainted piece of the screen: remembers the box it
/// painted and a fingerprint of what it painted there. All the logic lives in
/// FramePlan; this is just the state that survives between frames.
class Element {
public:
    const Rect& box() const { return box_; }

private:
    friend class FramePlan;
    Rect box_{};
    uint32_t fingerprint_ = 0;
    bool valid_ = false;
};

/// Works out what one frame has to repaint, and erases it.
///
/// The renderer never clears the whole framebuffer; last frame's pixels are
/// still there. So every element is registered with the box and fingerprint it
/// would paint this frame, and resolve() then
///
///   1. marks the elements whose box or content changed,
///   2. widens that set to any element an erasure would clip, because elements
///      from different layers can overlap once one of them moves,
///   3. performs every erasure, and only then
///   4. lets the caller paint.
///
/// Erasing everything before painting anything is what stops one element from
/// wiping out a neighbour that has already painted this frame. The panel's own
/// dirty-rect tracking then sends just the union of the touched boxes.
class FramePlan {
public:
    static constexpr int Capacity = 12;

    void begin(bool repaintAll);

    /// Registers an element. Returns a handle for shouldPaint(), or -1 if full.
    int add(Element& element, const Rect& box, uint32_t fingerprint);

    /// Decides, erases, and records the new state. Call once, after every add.
    void resolve(Gfx& gfx, uint16_t background = 0x0000);

    bool shouldPaint(int handle) const;
    bool anyPaint() const { return anyPaint_; }

private:
    struct Entry {
        Element* element;
        Rect box;
        Rect erase;
        uint32_t fingerprint;
        bool paint;
    };
    Entry entries_[Capacity]{};
    int count_ = 0;
    bool repaintAll_ = false;
    bool anyPaint_ = false;
};

}  // namespace ui
