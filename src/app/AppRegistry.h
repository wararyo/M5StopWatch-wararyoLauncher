#pragma once
#include <array>
namespace launcher {
enum class AppId { Stopwatch,Settings,External1,External2,External3 };
enum class TargetKind { Builtin,External };
struct AppEntry { AppId id; const char* name; TargetKind kind; int slot; bool available; const char* reason; };
inline constexpr std::array<AppEntry,5> AppRegistry{{
    {AppId::Stopwatch,"ストップウォッチ",TargetKind::Builtin,-1,false,"未実装"},
    {AppId::Settings,"設定",TargetKind::Builtin,-1,false,"未実装"},
    {AppId::External1,"外部アプリ1",TargetKind::External,1,false,"未確認"},
    {AppId::External2,"外部アプリ2",TargetKind::External,2,false,"未確認"},
    {AppId::External3,"外部アプリ3",TargetKind::External,3,false,"未確認"}
}};
}
