#pragma once
#include <array>
namespace launcher {
enum class AppId { Stopwatch, Settings, External1, External2, External3 };
enum class TargetKind { Builtin, External };
struct AppEntry { AppId id; const char* name; TargetKind kind; int slot; bool available; };
inline constexpr std::array<AppEntry, 5> AppRegistry{{
    {AppId::Stopwatch, "Stopwatch", TargetKind::Builtin, -1, false},
    {AppId::Settings, "Settings", TargetKind::Builtin, -1, false},
    {AppId::External1, "External 1", TargetKind::External, 1, false},
    {AppId::External2, "External 2", TargetKind::External, 2, false},
    {AppId::External3, "External 3", TargetKind::External, 3, false}
}};
}
