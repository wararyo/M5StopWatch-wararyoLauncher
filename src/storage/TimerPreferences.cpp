#include "TimerPreferences.h"
#include "services/TimerService.h"
namespace launcher {
size_t TimerPreferences::encode(int32_t seconds,uint8_t* out) {
    const uint32_t value=uint32_t(seconds);
    out[0]=Format;
    for (int i=0;i<4;++i) out[1+i]=uint8_t(value>>(8*i));
    return RecordBytes;
}
bool TimerPreferences::decode(const uint8_t* in,size_t size,int32_t& seconds) {
    if (size!=RecordBytes || in[0]!=Format) return false;
    uint32_t value=0;
    for (int i=0;i<4;++i) value|=uint32_t(in[1+i])<<(8*i);
    if (value<1 || value>uint32_t(TimerMaxSeconds)) return false;
    seconds=int32_t(value);
    return true;
}
PrefResult TimerPreferences::load() {
    value_=DefaultSeconds; saved_=-1;
    if (!backend_) return PrefResult::Unavailable;
    uint8_t record[RecordBytes+1];
    size_t size=sizeof(record);
    const auto read=backend_->read(Key,record,size);
    if (read!=PrefResult::Ok) return read;
    int32_t seconds=0;
    if (!decode(record,size,seconds)) return PrefResult::Invalid;
    value_=saved_=seconds;
    return PrefResult::Ok;
}
PrefResult TimerPreferences::remember(int32_t seconds) {
    if (seconds<1 || seconds>TimerMaxSeconds) return PrefResult::TooLarge;
    value_=seconds;
    if (seconds==saved_) return PrefResult::Ok;
    if (!backend_) return PrefResult::Unavailable;
    uint8_t record[RecordBytes];
    const auto result=backend_->write(Key,record,encode(seconds,record));
    if (result==PrefResult::Ok) saved_=seconds;
    return result;
}
}
