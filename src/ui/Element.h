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
    // Current maximum: 5 watch elements + 5 rows + hint + toast = 12.
    // Extra room is for future faces; overflow is handled, never truncated.
    static constexpr int Capacity=32;
    void begin(bool full,int limit=Capacity) {
        count_=0; full_=full; overflow_=false; limit_=std::clamp(limit,0,Capacity);
    }
    int add(Element& element,Rect box,uint32_t fingerprint);
    void resolve();
    bool shouldPaint(int handle) const { return full_ || (handle>=0 && handle<count_ && entries_[handle].paint); }
    bool full() const { return full_; }
    bool overflow() const { return overflow_; }
    bool anyPaint() const;
    int count() const { return count_; }
    Rect eraseBox(int i) const { return entries_[i].paint ? entries_[i].oldBox : Rect{}; }
private:
    struct Entry { Element* element=nullptr; Rect box{},oldBox{}; uint32_t fingerprint=0; bool paint=false; };
    std::array<Entry,Capacity> entries_{};
    int count_=0,limit_=Capacity;
    bool full_=true,overflow_=false;
};
}
