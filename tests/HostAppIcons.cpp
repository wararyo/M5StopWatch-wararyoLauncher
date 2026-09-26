#include "assets/AppIcons.h"
#include <array>
namespace launcher {
// The host tests have no embedded AppIcons.bin (src/assets/AppIcons.cpp links
// against it), so the set is stood in for here: one small, distinct, static
// mask per id, as the firmware's is static for the life of the app.
const IconBitmap* appIcon(IconId id) {
    static const uint8_t pixels[static_cast<int>(IconId::Count)][4]={
        {0,255,255,0},{255,0,0,255},{255,255,0,0},{0,0,255,255},{128,128,128,128}};
    static const std::array<IconBitmap,static_cast<int>(IconId::Count)> set=[] {
        std::array<IconBitmap,static_cast<int>(IconId::Count)> icons{};
        for (int i=0;i<static_cast<int>(IconId::Count);++i) icons[i]={pixels[i],2,2};
        return icons;
    }();
    const int index=static_cast<int>(id);
    return index>=0 && index<static_cast<int>(IconId::Count) ? &set[index] : nullptr;
}
}
