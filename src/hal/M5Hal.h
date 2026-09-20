#pragma once
#include "Hal.h"
namespace launcher {
class M5Hal final : public Hal {
public:
    TimeUs now() override;
    InputSnapshot sampleInput() override;
    UsbState sampleUsb() override;
    void setScreenOff(bool off) override;
    void waitUs(TimeUs delay) override;
};
void beginRuntimeDiagnostics();
void runtimeDiagnostics();
}
