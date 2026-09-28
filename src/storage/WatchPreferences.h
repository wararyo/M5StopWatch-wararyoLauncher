#pragma once
#include <cstddef>
#include <cstdint>
namespace launcher {
// The watch face records (docs/task10/plan-10-5.md 4): which face is selected,
// and each face's own state, as records of their own beside the settings
// record, which stays exactly as it was. No display or NVS types, so the host
// tests run the formats and the separation as they are.
enum class PrefResult : uint8_t {
    Ok,
    Missing,      // No such record yet.
    Invalid,      // A record that is there but cannot be used: unknown format, bad length.
    Unavailable,  // Storage could not be read or opened at all.
    WriteFailed,
    TooLarge,     // Refused before writing: over the record's size limit.
};
const char* prefResultName(PrefResult result);
// Blobs by key, in one namespace. The NVS implementation is the only code
// that touches NVS (storage/NvsBackend.h).
class RecordBackend {
public:
    virtual ~RecordBackend()=default;
    // `size` is the capacity on the way in and the length read on the way out.
    virtual PrefResult read(const char* key,uint8_t* data,size_t& size)=0;
    virtual PrefResult write(const char* key,const uint8_t* data,size_t size)=0;
};
// NVS keys are at most 15 characters. Face records use lower case letters,
// digits and '_', so a key can never be mistaken for another component's.
constexpr size_t RecordKeyMax=15;
constexpr size_t WatchFaceIdMax=31;
constexpr size_t FacePayloadMax=64;
bool validRecordKey(const char* key);
bool validWatchFaceId(const char* id);
// Keys owned by the host, which no face may take.
bool reservedRecordKey(const char* key);
class WatchPreferences {
public:
    // Explicit layouts, never a struct in memory:
    //   selection: [0] format 1  [1] id length n (1..31)  [2..2+n) the id
    //   face:      [0] format 1  [1] payload length n (0..64)  [2..2+n) payload
    // The payload's meaning and its own version are the face's.
    static constexpr uint8_t Format=1;
    static constexpr const char* SelectionKey="watch_sel";
    static constexpr size_t SelectionBytes=2+WatchFaceIdMax,FaceBytes=2+FacePayloadMax;
    void bind(RecordBackend* backend) { backend_=backend; }
    bool bound() const { return backend_!=nullptr; }
    // `id` receives a terminated copy; it needs WatchFaceIdMax+1 bytes.
    PrefResult loadSelection(char* id,size_t capacity) const;
    PrefResult saveSelection(const char* id);
    PrefResult loadFace(const char* key,uint8_t* payload,size_t capacity,size_t& size) const;
    PrefResult saveFace(const char* key,const uint8_t* payload,size_t size);
    static size_t encode(const uint8_t* body,size_t size,uint8_t* out);
    static PrefResult decode(const uint8_t* in,size_t size,size_t limit,const uint8_t*& body,size_t& length);
private:
    RecordBackend* backend_=nullptr;
};
// One face's record, and nothing else: the key is fixed by the host when the
// face is registered, so a face cannot name another face's record or the
// settings (docs/task10/plan-10-5.md 4).
class FacePreferences {
public:
    FacePreferences()=default;
    FacePreferences(WatchPreferences* store,const char* key):store_(store),key_(key) {}
    bool bound() const { return store_ && store_->bound() && key_; }
    PrefResult load(uint8_t* payload,size_t capacity,size_t& size) const;
    PrefResult save(const uint8_t* payload,size_t size);
private:
    WatchPreferences* store_=nullptr;
    const char* key_=nullptr;
};
}
