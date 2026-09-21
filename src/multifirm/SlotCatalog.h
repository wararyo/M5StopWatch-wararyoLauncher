#pragma once
#include <cstdint>
namespace launcher {
// Guest slots are ota_1..ota_3, numbered the way MultiFirm numbers them.
inline constexpr int SlotCount=3;
// The launcher's own view of a slot. It is MultiFirm's SlotState plus the two
// states the library has no word for: not looked at yet, and a device whose
// partition table is not the MultiFirm layout at all. Keeping a separate enum
// is what lets the screens and their tests build without ESP-IDF.
enum class SlotStatus : uint8_t { Scanning, Ready, Empty, Invalid, ReadError, Unsupported };
struct SlotEntry {
    SlotStatus status=SlotStatus::Scanning;
    // Only meaningful for Ready. MultiFirm picks metadata -> project_name ->
    // "App{n}"; the launcher just shows what it decided.
    char name[32]{};
    char version[24]{};
    // esp_err_t of the failure, shown as a diagnostic. 0 when there is none.
    int32_t error=0;
};
struct SlotCatalog {
    bool layoutSupported=true;
    SlotEntry slots[SlotCount]{};
};
inline bool slotReady(const SlotCatalog& c,int slot) {
    return slot>=1 && slot<=SlotCount && c.slots[slot-1].status==SlotStatus::Ready;
}
}
