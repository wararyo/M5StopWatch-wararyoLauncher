#pragma once
#include <array>
#include <cstdint>
namespace launcher {
enum class AppId { Stopwatch,Settings,External1,External2,External3 };
enum class TargetKind { Builtin,External };
// Index into the embedded mask set (tools/build_icons.py writes this order).
// Entries are appended, never reordered: the asset is indexed by this value.
enum class IconId : uint8_t { Stopwatch,Settings,App1,App2,App3,Count };
struct AppEntry { AppId id; const char* name; IconId icon; TargetKind kind; int slot; };
inline constexpr std::array<AppEntry,5> AppRegistry{{
    {AppId::Stopwatch,"ストップウォッチ",IconId::Stopwatch,TargetKind::Builtin,-1},
    {AppId::Settings,"設定",IconId::Settings,TargetKind::Builtin,-1},
    {AppId::External1,"外部アプリ1",IconId::App1,TargetKind::External,1},
    {AppId::External2,"外部アプリ2",IconId::App2,TargetKind::External,2},
    {AppId::External3,"外部アプリ3",IconId::App3,TargetKind::External,3}
}};
}
