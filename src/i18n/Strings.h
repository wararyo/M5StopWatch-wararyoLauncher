#pragma once
// Every fixed string the launcher puts on screen, in the language chosen at
// build time: Japanese by default, English with -DLAUNCHER_LANGUAGE_EN=1
// (env:m5stopwatch-en). Both languages sit on one line so that a string is
// never added to one and forgotten in the other.
//
// A format carries its own word order and unit, since those differ between
// languages: "%d秒" is "%ds", not a number followed by a separate word.
// Names that come from elsewhere (guest images, watch faces) are not here.
//
// tools/build_font.py collects the non-ASCII characters of src/, so a new
// Japanese string here only needs the font subset rebuilt.
#define LAUNCHER_TEXTS(X) \
    /* App list. The external names stand in until a guest image names itself. */ \
    X(Stopwatch,          "ストップウォッチ",       "Stopwatch") \
    X(Timer,              "タイマー",               "Timer") \
    X(Pedometer,          "歩数計",                 "Pedometer") \
    X(Settings,           "設定",                   "Settings") \
    X(ExternalApp1,       "外部アプリ1",            "App 1") \
    X(ExternalApp2,       "外部アプリ2",            "App 2") \
    X(ExternalApp3,       "外部アプリ3",            "App 3") \
    X(ExternalAppFormat,  "外部アプリ%d",           "App %d") \
    /* Toasts */ \
    X(Unavailable,        "準備中",                 "Unavailable") \
    X(Saved,              "保存しました",           "Saved") \
    X(SaveFailed,         "保存に失敗しました",     "Could not save") \
    X(TimeSaved,          "時刻を保存しました",     "Time saved") \
    X(InvalidDate,        "日付が正しくありません", "Invalid date") \
    X(ClockUnavailable,   "時計を設定できません",   "Cannot set the clock") \
    /* Timer */ \
    X(Dismiss,            "解除",                   "Dismiss") \
    /* Pedometer. The unit follows the count. */ \
    X(StepsToday,         "今日の歩数",             "Steps today") \
    X(StepsUnit,          "歩",                     "steps") \
    X(FaceUnavailable,    "文字盤を表示できません", "Cannot show this face") \
    /* Settings */ \
    X(DateTime,           "日時",                   "Date & time") \
    X(Brightness,         "輝度",                   "Brightness") \
    X(ScreenOff,          "消灯時間",               "Screen off") \
    X(Info,               "情報",                   "Info") \
    X(WatchFace,          "文字盤",                 "Watch face") \
    X(SecondsFormat,      "%d秒",                   "%ds") \
    X(InUseFormat,        "%s  使用中",             "%s  (in use)") \
    X(ShowStatistics,     "統計情報を表示",         "Show statistics") \
    X(Save,               "保存",                   "Save") \
    X(Cancel,             "キャンセル",             "Cancel") \
    X(Back,               "戻る",                   "Back") \
    /* External app slots */ \
    X(SlotFormat,         "スロット%d",             "Slot %d") \
    X(SlotScanning,       "検証中",                 "Checking") \
    X(SlotEmpty,          "空き",                   "Empty") \
    X(SlotInvalid,        "破損",                   "Corrupted") \
    X(SlotReadError,      "読み取り失敗",           "Read error") \
    X(SlotUnsupported,    "非対応",                 "Unsupported") \
    X(Launching,          "起動中",                 "Launching") \
    X(VersionFormat,      "バージョン %s",          "Version %s") \
    X(LaunchFailedFormat, "起動できませんでした %s", "Launch failed %s") \
    X(ErrorFormat,        "エラー 0x%x",            "Error 0x%x")

namespace launcher::text {
#if LAUNCHER_LANGUAGE_EN
#define LAUNCHER_TEXT_DEFINE(name,ja,en) inline constexpr const char* name=en;
#else
#define LAUNCHER_TEXT_DEFINE(name,ja,en) inline constexpr const char* name=ja;
#endif
LAUNCHER_TEXTS(LAUNCHER_TEXT_DEFINE)
#undef LAUNCHER_TEXT_DEFINE
// All of the above, for checks that every one of them can be drawn.
#define LAUNCHER_TEXT_NAME(name,ja,en) name,
inline constexpr const char* All[]={LAUNCHER_TEXTS(LAUNCHER_TEXT_NAME)};
#undef LAUNCHER_TEXT_NAME
}
