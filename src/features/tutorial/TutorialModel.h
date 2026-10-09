#pragma once
#include "features/settings/SettingsModel.h"
#include <cstdint>
namespace launcher {
// The tutorial's steps (docs/task15/plan.md 1.1): the date, only while the
// clock is unset, then the way home.
enum class TutorialStep : uint8_t { DateTime, Guide };
struct TutorialModel {
    TutorialStep step=TutorialStep::Guide;
    // The date step's editor, drawn as settings draws its own.
    SettingsEditorModel editor{};
};
}
