#pragma once
#include "storage/Settings.h"
namespace launcher {
struct EffectiveSettings {
    int brightness=Settings{}.brightness;
    int screenOffSec=Settings{}.screenOffSec;
};
// Choices that last until the next restart and are never stored. Owned by the
// application; a screen may only ask for a change (ScreenOutcome).
struct RuntimeSettings { bool stats=false; };
}
