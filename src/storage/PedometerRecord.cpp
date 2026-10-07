#include "PedometerRecord.h"
namespace launcher {
size_t PedometerRecord::encode(int32_t day,uint32_t steps,uint8_t* out) {
    const uint32_t d=uint32_t(day);
    out[0]=Format;
    for (int i=0;i<4;++i) { out[1+i]=uint8_t(d>>(8*i)); out[5+i]=uint8_t(steps>>(8*i)); }
    return RecordBytes;
}
bool PedometerRecord::decode(const uint8_t* in,size_t size,int32_t& day,uint32_t& steps) {
    if (size!=RecordBytes || in[0]!=Format) return false;
    uint32_t d=0,s=0;
    for (int i=0;i<4;++i) { d|=uint32_t(in[1+i])<<(8*i); s|=uint32_t(in[5+i])<<(8*i); }
    day=int32_t(d); steps=s;
    return true;
}
PrefResult PedometerRecord::load() {
    has_=false;
    if (!backend_) return PrefResult::Unavailable;
    uint8_t record[RecordBytes+1];
    size_t size=sizeof(record);
    const auto read=backend_->read(Key,record,size);
    if (read!=PrefResult::Ok) return read;
    int32_t day=0; uint32_t steps=0;
    if (!decode(record,size,day,steps)) return PrefResult::Invalid;
    has_=true; day_=day; steps_=steps;
    return PrefResult::Ok;
}
PrefResult PedometerRecord::remember(int32_t day,uint32_t steps) {
    if (has_ && day==day_ && steps==steps_) return PrefResult::Ok;
    if (!backend_) return PrefResult::Unavailable;
    uint8_t record[RecordBytes];
    const auto result=backend_->write(Key,record,encode(day,steps,record));
    if (result==PrefResult::Ok) { has_=true; day_=day; steps_=steps; }
    return result;
}
}
