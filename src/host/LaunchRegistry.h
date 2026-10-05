#pragma once
#include "core/AppId.h"
#include "assets/AppIcons.h"
#include "i18n/Strings.h"
#include <array>
#include <cstdint>
namespace launcher {
enum class TargetKind { Builtin,External };
struct LaunchEntry { LaunchTargetId id; const char* name; IconId icon; TargetKind kind; int slot; };
inline constexpr std::array<LaunchEntry,5> LaunchRegistry{{
    {LaunchTargetId::Stopwatch,text::Stopwatch,IconId::Stopwatch,TargetKind::Builtin,-1},
    {LaunchTargetId::Settings,text::Settings,IconId::Settings,TargetKind::Builtin,-1},
    {LaunchTargetId::External1,text::ExternalApp1,IconId::App1,TargetKind::External,1},
    {LaunchTargetId::External2,text::ExternalApp2,IconId::App2,TargetKind::External,2},
    {LaunchTargetId::External3,text::ExternalApp3,IconId::App3,TargetKind::External,3}
}};
}
