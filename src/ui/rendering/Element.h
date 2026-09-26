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
class FramePlan {
public:
    // Current maximum: 8 watch elements + 8 list slots (ListVisibleSlots,
    // registered even while hidden so they erase) + toast + the open app
    // screen (11 settings editor elements or the settings menu's 8 list
    // slots, 7 stopwatch, 6 external app detail) = 28. A closed screen, and
    // the half of settings that is not shown, registers nothing, so they
    // never add up.
    // Extra room is for future faces; overflow is handled, never truncated.
    static constexpr int Capacity=32;
    void begin(bool full,int limit=Capacity) {
        count_=0; full_=full; overflow_=false; limit_=std::clamp(limit,0,Capacity);
    }
    int add(Element& element,Rect box,uint32_t fingerprint);
    // For a part that cannot describe this frame by its elements alone: a
    // list drawing more rows than it has slots, a notice coming or going. Any
    // time before resolve(); every element then paints over a cleared screen.
    void forceFull() { full_=true; }
    void resolve();
    bool shouldPaint(int handle) const { return full_ || (handle>=0 && handle<count_ && entries_[handle].paint); }
    bool full() const { return full_; }
    bool overflow() const { return overflow_; }
    bool anyPaint() const;
    int count() const { return count_; }
    Rect eraseBox(int i) const { return entries_[i].paint ? entries_[i].oldBox : Rect{}; }
    // Bounding box of everything this frame erases and repaints, valid after
    // resolve(). Empty on a full repaint, where the caller already knows the
    // answer is the whole screen. This panel flushes one bounding box per
    // frame, so anything drawn outside this rectangle enlarges the transfer.
    Rect dirtyBounds() const { return dirty_; }
private:
    struct Entry { Element* element=nullptr; Rect box{},oldBox{}; uint32_t fingerprint=0; bool paint=false; };
    std::array<Entry,Capacity> entries_{};
    Rect dirty_{};
    int count_=0,limit_=Capacity;
    bool full_=true,overflow_=false;
};
}
