#include "WatchPreferences.h"
#include <cstring>
namespace launcher {
const char* prefResultName(PrefResult result) {
    switch (result) {
    case PrefResult::Ok: return "ok";
    case PrefResult::Missing: return "missing";
    case PrefResult::Invalid: return "invalid";
    case PrefResult::Unavailable: return "unavailable";
    case PrefResult::WriteFailed: return "write_failed";
    case PrefResult::TooLarge: return "too_large";
    }
    return "?";
}
bool validRecordKey(const char* key) {
    if (!key) return false;
    const size_t length=std::strlen(key);
    if (length==0 || length>RecordKeyMax) return false;
    for (size_t i=0;i<length;++i) {
        const char c=key[i];
        if (!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='_')) return false;
    }
    return true;
}
bool validWatchFaceId(const char* id) {
    if (!id) return false;
    const size_t length=std::strlen(id);
    if (length==0 || length>WatchFaceIdMax) return false;
    for (size_t i=0;i<length;++i) if (id[i]<=' ' || id[i]>'~') return false;
    return true;
}
bool reservedRecordKey(const char* key) {
    // The selection, the settings record of storage/NvsBackend.cpp and the
    // timer's last length (storage/TimerPreferences.h).
    return key && (std::strcmp(key,WatchPreferences::SelectionKey)==0 || std::strcmp(key,"config")==0 ||
                   std::strcmp(key,"timer")==0);
}
size_t WatchPreferences::encode(const uint8_t* body,size_t size,uint8_t* out) {
    out[0]=Format; out[1]=uint8_t(size);
    if (size) std::memcpy(out+2,body,size);
    return 2+size;
}
PrefResult WatchPreferences::decode(const uint8_t* in,size_t size,size_t limit,const uint8_t*& body,size_t& length) {
    if (size<2 || in[0]!=Format || in[1]>limit || size!=size_t(2+in[1])) return PrefResult::Invalid;
    body=in+2; length=in[1];
    return PrefResult::Ok;
}
PrefResult WatchPreferences::loadSelection(char* id,size_t capacity) const {
    if (capacity) id[0]=0;
    if (!backend_) return PrefResult::Unavailable;
    uint8_t record[SelectionBytes+1];
    size_t size=sizeof(record);
    const auto read=backend_->read(SelectionKey,record,size);
    if (read!=PrefResult::Ok) return read;
    const uint8_t* body=nullptr; size_t length=0;
    if (decode(record,size,WatchFaceIdMax,body,length)!=PrefResult::Ok || length+1>capacity) return PrefResult::Invalid;
    std::memcpy(id,body,length); id[length]=0;
    if (!validWatchFaceId(id)) { id[0]=0; return PrefResult::Invalid; }
    return PrefResult::Ok;
}
PrefResult WatchPreferences::saveSelection(const char* id) {
    if (!validWatchFaceId(id)) return PrefResult::TooLarge;
    if (!backend_) return PrefResult::Unavailable;
    uint8_t record[SelectionBytes];
    const size_t size=encode(reinterpret_cast<const uint8_t*>(id),std::strlen(id),record);
    return backend_->write(SelectionKey,record,size);
}
PrefResult WatchPreferences::loadFace(const char* key,uint8_t* payload,size_t capacity,size_t& size) const {
    size=0;
    if (!validRecordKey(key) || reservedRecordKey(key)) return PrefResult::Invalid;
    if (!backend_) return PrefResult::Unavailable;
    uint8_t record[FaceBytes+1];
    size_t read=sizeof(record);
    const auto result=backend_->read(key,record,read);
    if (result!=PrefResult::Ok) return result;
    const uint8_t* body=nullptr; size_t length=0;
    if (decode(record,read,FacePayloadMax,body,length)!=PrefResult::Ok || length>capacity) return PrefResult::Invalid;
    if (length) std::memcpy(payload,body,length);
    size=length;
    return PrefResult::Ok;
}
PrefResult WatchPreferences::saveFace(const char* key,const uint8_t* payload,size_t size) {
    if (!validRecordKey(key) || reservedRecordKey(key)) return PrefResult::Invalid;
    if (size>FacePayloadMax) return PrefResult::TooLarge;
    if (!backend_) return PrefResult::Unavailable;
    uint8_t record[FaceBytes];
    return backend_->write(key,record,encode(payload,size,record));
}
PrefResult FacePreferences::load(uint8_t* payload,size_t capacity,size_t& size) const {
    size=0;
    return bound() ? store_->loadFace(key_,payload,capacity,size) : PrefResult::Unavailable;
}
PrefResult FacePreferences::save(const uint8_t* payload,size_t size) {
    return bound() ? store_->saveFace(key_,payload,size) : PrefResult::Unavailable;
}
}
