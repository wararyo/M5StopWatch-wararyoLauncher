#include "TutorialRecord.h"
namespace launcher {
PrefResult TutorialRecord::load() {
    due_=false;
    if (!backend_) return PrefResult::Unavailable;
    uint8_t record[RecordBytes+1];
    size_t size=sizeof(record);
    const auto read=backend_->read(Key,record,size);
    if (read==PrefResult::Missing) due_=true;
    if (read!=PrefResult::Ok) return read;
    return size==RecordBytes && record[0]==Format ? PrefResult::Ok : PrefResult::Invalid;
}
PrefResult TutorialRecord::complete() {
    if (!backend_) return PrefResult::Unavailable;
    const uint8_t record[RecordBytes]={Format};
    const auto result=backend_->write(Key,record,sizeof(record));
    if (result==PrefResult::Ok) due_=false;
    return result;
}
}
