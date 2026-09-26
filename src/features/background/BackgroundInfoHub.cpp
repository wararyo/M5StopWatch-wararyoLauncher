#include "BackgroundInfoHub.h"
#include <cstring>
namespace launcher {
namespace {
const BackgroundInfo* find(const BackgroundSnapshot& s,LaunchTargetId id) {
    for (int i=0;i<s.count;++i) if (s.items[i].appId==id) return &s.items[i];
    return nullptr;
}
// Bytes the character starting with `lead` claims; an invalid lead counts as one.
int utf8Length(unsigned char lead) {
    if (lead<0x80) return 1;
    if ((lead&0xe0)==0xc0) return 2;
    if ((lead&0xf0)==0xe0) return 3;
    if ((lead&0xf8)==0xf0) return 4;
    return 1;
}
}
int sanitizeBackgroundLabel(char (&label)[BackgroundLabelBytes]) {
    int length=0;
    while (length<BackgroundLabelBytes-1 && label[length]) ++length;
    // Back over the last character's continuation bytes to its lead, and drop
    // it when the bytes it claims do not all fit.
    if (length>0) {
        int lead=length-1;
        while (lead>0 && (static_cast<unsigned char>(label[lead])&0xc0)==0x80) --lead;
        if (lead+utf8Length(static_cast<unsigned char>(label[lead]))>length) length=lead;
    }
    std::memset(label+length,0,BackgroundLabelBytes-length);
    return length;
}
BackgroundInfoHub::AddResult BackgroundInfoHub::add(const BackgroundInfoProvider& provider) {
    for (int i=0;i<providerCount_;++i)
        if (providers_[i]->id()==provider.id()) { ++rejected_; return AddResult::Duplicate; }
    if (providerCount_>=BackgroundCapacity) { ++rejected_; return AddResult::Full; }
    providers_[providerCount_++]=&provider;
    return AddResult::Added;
}
uint8_t BackgroundInfoHub::collect(TimeUs now) {
    scratch_.count=0;
    for (int i=0;i<providerCount_;++i) {
        auto& item=scratch_.items[scratch_.count];
        item=BackgroundInfo{};
        item.appId=providers_[i]->id();
        if (!providers_[i]->sample(now,item)) continue;
        // The id is the one the provider registered under, whatever it wrote.
        item.appId=providers_[i]->id();
        item.icon=usableIcon(item.icon);
        if (sanitizeBackgroundLabel(item.label)>0) ++scratch_.count;
    }
    uint8_t changes=BackgroundUnchanged;
    for (int i=0;i<snapshot_.count;++i) {
        const auto* next=find(scratch_,snapshot_.items[i].appId);
        if (!next) changes|=BackgroundRemoved;
        else {
            if (std::strcmp(next->label,snapshot_.items[i].label)!=0) changes|=BackgroundRelabeled;
            if (!sameStyle(*next,snapshot_.items[i])) changes|=BackgroundRestyled;
        }
    }
    for (int i=0;i<scratch_.count;++i)
        if (!find(snapshot_,scratch_.items[i].appId)) changes|=BackgroundAdded;
    snapshot_=scratch_;
    pending_=false;
    return changes;
}
}
