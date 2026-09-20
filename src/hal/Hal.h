#pragma once
#include "app/ScreenManager.h"
#include "power/PowerManager.h"
namespace launcher {
class Hal {
public:
    virtual ~Hal() = default;
    virtual TimeUs now() = 0;
    virtual InputSnapshot sampleInput() = 0;
    virtual UsbState sampleUsb() = 0;
    virtual void setScreenOff(bool off) = 0;
    virtual void draw(const ScreenModel&, const UsbState&) = 0;
    virtual void waitUs(TimeUs delay) = 0;
};
}
