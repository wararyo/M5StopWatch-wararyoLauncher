#pragma once
#include "core/AppId.h"
#include "assets/AppIcons.h"
#include <array>
#include <cstdint>
namespace launcher {
enum class TargetKind { Builtin,External };
struct LaunchEntry { LaunchTargetId id; const char* name; IconId icon; TargetKind kind; int slot; };
inline constexpr std::array<LaunchEntry,5> LaunchRegistry{{
    {LaunchTargetId::Stopwatch,"ストップウォッチ",IconId::Stopwatch,TargetKind::Builtin,-1},
    {LaunchTargetId::Settings,"設定",IconId::Settings,TargetKind::Builtin,-1},
    {LaunchTargetId::External1,"外部アプリ1",IconId::App1,TargetKind::External,1},
    {LaunchTargetId::External2,"外部アプリ2",IconId::App2,TargetKind::External,2},
    {LaunchTargetId::External3,"外部アプリ3",IconId::App3,TargetKind::External,3}
}};
}
