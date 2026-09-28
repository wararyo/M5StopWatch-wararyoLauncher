#pragma once
#include "Geometry.h"
#include <array>
#include <cstdint>
namespace launcher {
inline uint32_t hashString(const char* s,uint32_t h=2166136261u) {
    while (*s) { h^=static_cast<uint8_t>(*s++); h*=16777619u; } return h;
}
inline uint32_t hashValue(uint32_t v,uint32_t h=2166136261u) {
    for(int i=0;i<4;++i) { h^=(v>>(i*8))&255; h*=16777619u; } return h;
}
struct Element { Rect box{}; uint32_t fingerprint=0; bool valid=false; };
// One frame's plan (docs/task10/plan-10-4.md 1-2). Elements are compared with
// what they were last frame; the old and new boxes of every element that
// changed, and whatever a layer declares with damage(), make up the frame's
// damage: one rectangle, since this panel flushes one bounding box per frame.
// Inside it the background is restored and every element that reaches it is
// painted again, back to front, clipped to it. Nothing outside it is touched,
// so a large background or foreground that merely overlaps a small change
// never widens the damage.
class FramePlan {
public:
    // Current maximum: 8 watch elements + 8 list slots (ListVisibleSlots,
    // registered even while hidden so they erase) + toast + the open app
    // screen (11 settings editor elements or the settings menu's 8 list
    // slots, 7 stopwatch, 6 external app detail) = 28. A closed screen, and
    // the half of settings that is not shown, registers nothing, so they
    // never add up. Backgrounds declare damage instead of registering.
    // Extra room is for future faces; overflow is handled, never truncated.
    static constexpr int Capacity=32;
    // `bounds` is the screen: a full repaint covers it, and damage is kept inside it.
    void begin(bool full,Rect bounds,int limit=Capacity) {
        count_=0; full_=full; overflow_=false; limit_=std::clamp(limit,0,Capacity);
        bounds_=bounds; declared_={}; damage_={};
    }
    int add(Element& element,Rect box,uint32_t fingerprint);
    // Pixels that change although no element did: a background whose colour
    // or extent moved. Any time before resolve().
    void damage(Rect area) { declared_=unite(declared_,area); }
    // For a part that cannot describe this frame by its elements alone: a
    // list drawing more rows than it has slots, a face just selected. Any
    // time before resolve(); every element then paints over a cleared screen.
    void forceFull() { full_=true; }
    void resolve();
    // After resolve(): whether the element reaches the area this frame paints.
    // Always true on a full repaint, including for a handle of -1.
    bool shouldPaint(int handle) const { return full_ || (handle>=0 && handle<count_ && entries_[handle].paint); }
    bool full() const { return full_; }
    bool overflow() const { return overflow_; }
    bool anyPaint() const { return full_ || !damage_.empty(); }
    int count() const { return count_; }
    // What this frame restores and paints, valid after resolve(): the screen
    // on a full repaint, the damage otherwise, empty when nothing changed.
    Rect area() const { return full_ ? bounds_ : damage_; }
private:
    struct Entry { Element* element=nullptr; Rect box{},oldBox{}; uint32_t fingerprint=0; bool changed=false,paint=false; };
    std::array<Entry,Capacity> entries_{};
    Rect bounds_{},declared_{},damage_{};
    int count_=0,limit_=Capacity;
    bool full_=true,overflow_=false;
};
}
