#pragma once
#include "storage/Settings.h"
namespace launcher {
struct EffectiveSettings {
    int brightness=Settings{}.brightness;
    int screenOffSec=Settings{}.screenOffSec;
};
struct RuntimeSettings { bool stats=false; };
}
