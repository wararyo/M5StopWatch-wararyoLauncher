#pragma once
#include "multifirm/SlotCatalog.h"
namespace launcher {
enum class ExternalPhase : uint8_t { Browsing, BootCommitting, BootFailed };
struct ExternalModel {
    int slot=1;
    SlotStatus status=SlotStatus::Scanning;
    ExternalPhase phase=ExternalPhase::Browsing;
    int cursor=0;
    const char* name=nullptr;
    const char* version=nullptr;
    const char* message=nullptr;
    int32_t error=0;
};
}
