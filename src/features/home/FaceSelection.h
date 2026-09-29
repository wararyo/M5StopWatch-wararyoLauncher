#pragma once
#include "features/home/HomeInteraction.h"
#include "storage/WatchPreferences.h"
#include <array>
#include <cstdio>
#include <cstring>
namespace launcher {
// A face as registered: its stable id, the name settings shows and the key of
// its own record (nullptr: it keeps nothing across restarts).
struct FaceDescriptor {
    const char* id=nullptr;
    const char* name=nullptr;
    const char* storageKey=nullptr;
};
// Which faces exist, which one shows, and the stored choice
// (docs/task10/plan-10-5.md 4-5). The clock layer begins and ends the faces;
// this decides what to begin and what to store, without any display type, so
// the host tests run it with stand-in faces.
//
// The choice is stored by the host under its own key, as the face's id. Each
// face gets a FacePreferences bound to its own key only, checked here when it
// is registered: a valid key, not the host's, not another face's.
class FaceSelection {
public:
    // The built-in faces. The render check registers its two test faces on
    // top of them; settings lists the first SettingsFaceCapacity either way.
#ifdef LAUNCHER_RENDER_DIAGNOSTICS
    static constexpr int Capacity=6;
#else
    static constexpr int Capacity=4;
#endif
    void bind(WatchPreferences* store) { store_=store; }
    // Refused: a full registry, a bad or duplicate id, a bad, reserved or
    // duplicate key. The returned port lives as long as this object.
    FacePreferences* add(const FaceDescriptor& face) {
        if (count_>=Capacity || !validWatchFaceId(face.id) || find(face.id)>=0) return nullptr;
        if (face.storageKey) {
            if (!validRecordKey(face.storageKey) || reservedRecordKey(face.storageKey)) return nullptr;
            for (int i=0;i<count_;++i)
                if (faces_[i].storageKey && std::strcmp(faces_[i].storageKey,face.storageKey)==0) return nullptr;
        }
        faces_[count_]=face;
        ports_[count_]=FacePreferences(face.storageKey ? store_ : nullptr,face.storageKey);
        return &ports_[count_++];
    }
    int count() const { return count_; }
    const FaceDescriptor& at(int i) const { return faces_[i]; }
    int find(const char* id) const {
        for (int i=0;i<count_;++i) if (id && std::strcmp(faces_[i].id,id)==0) return i;
        return -1;
    }
    int current() const { return current_; }
    // The face to begin at start: the stored one when it is registered, else
    // the first. Nothing is written: an unknown or broken record stays as it
    // is until the user chooses a face.
    int startup() {
        char id[WatchFaceIdMax+1]{};
        const auto result=store_ ? store_->loadSelection(id,sizeof(id)) : PrefResult::Unavailable;
        const int stored=result==PrefResult::Ok ? find(id) : -1;
        stored_=stored;
        std::printf("[WatchFace] stored selection=%s id=%s%s\n",prefResultName(result),id[0] ? id : "-",
                    result==PrefResult::Ok && stored<0 ? " (unknown, first face shown)" : "");
        return stored>=0 ? stored : 0;
    }
    // The face the clock layer managed to begin (at start, or after a failed
    // choice fell back). -1: none.
    void shown(int index) { current_=index; }
    // A confirmed choice. `begin(index)` switches the clock layer to that face
    // and says whether it began; on failure it has put the previous one back.
    template<class Begin> FaceChoiceResult choose(const char* id,Begin&& begin) {
        const int index=find(id);
        if (index<0) return FaceChoiceResult::Unknown;
        if (index==current_ && stored_==index) return FaceChoiceResult::Unchanged;
        // The face already showing is not begun again; only its storing,
        // which failed last time, is retried.
        if (index!=current_) {
            if (!begin(index)) return FaceChoiceResult::Failed;
            current_=index;
        }
        if (!store_ || store_->saveSelection(faces_[index].id)!=PrefResult::Ok) {
            std::printf("[WatchFace] selection %s shown but not stored\n",faces_[index].id);
            return FaceChoiceResult::SaveFailed;
        }
        stored_=index;
        return FaceChoiceResult::Selected;
    }
private:
    WatchPreferences* store_=nullptr;
    std::array<FaceDescriptor,Capacity> faces_{};
    std::array<FacePreferences,Capacity> ports_{};
    int count_=0,current_=-1;
    // Which face the stored record names, as far as this run knows; -1 for
    // none, unknown or unreadable.
    int stored_=-1;
};
}
