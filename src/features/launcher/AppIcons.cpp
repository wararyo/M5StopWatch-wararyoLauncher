#include "AppIcons.h"
#include <array>
#include <cstdio>
#include <cstring>

extern const uint8_t iconsStart[] asm("_binary_AppIcons_bin_start");
extern const uint8_t iconsEnd[] asm("_binary_AppIcons_bin_end");

namespace launcher {
namespace {
constexpr size_t HeaderBytes=12;
constexpr int Count=static_cast<int>(IconId::Count);
struct IconSet { std::array<IconBitmap,Count> icons{}; bool valid=false; };
// The header is little endian and read byte by byte: the embedded blob carries
// no alignment guarantee of its own.
uint16_t readField(const uint8_t* p) { return static_cast<uint16_t>(p[0]|(p[1]<<8)); }
const IconSet& load() {
    static const IconSet set=[]() -> IconSet {
        IconSet result;
        const size_t bytes=static_cast<size_t>(iconsEnd-iconsStart);
        if(bytes<HeaderBytes || std::memcmp(iconsStart,"LICN",4)!=0) {
            std::printf("[Icons] AppIcons.bin not recognised: bytes=%u\n",unsigned(bytes));
            return result;
        }
        const int version=readField(iconsStart+4),count=readField(iconsStart+6);
        const int width=readField(iconsStart+8),height=readField(iconsStart+10);
        const size_t needed=HeaderBytes+static_cast<size_t>(count)*width*height;
        if(version!=1 || count<Count || width<=0 || height<=0 || bytes<needed) {
            std::printf("[Icons] AppIcons.bin rejected: version=%d count=%d size=%dx%d bytes=%u\n",
                version,count,width,height,unsigned(bytes));
            return result;
        }
        for(int i=0;i<Count;++i)
            result.icons[i]={iconsStart+HeaderBytes+static_cast<size_t>(i)*width*height,width,height};
        result.valid=true;
        std::printf("[Icons] AppIcons count=%d size=%dx%d bytes=%u\n",count,width,height,unsigned(bytes));
        return result;
    }();
    return set;
}
}
const IconBitmap* appIcon(IconId id) {
    const auto& set=load();
    const int index=static_cast<int>(id);
    if(!set.valid || index<0 || index>=Count) return nullptr;
    return &set.icons[index];
}
}
