#pragma once
#include "power/PowerManager.h"
namespace launcher {
class Hal {
public:
    virtual ~Hal() = default;
    virtual TimeUs now() = 0;
    virtual InputSnapshot sampleInput() = 0;
    virtual UsbState sampleUsb() = 0;
    virtual void setScreenOff(bool off) = 0;
    virtual void waitUs(TimeUs delay) = 0;
};
}
