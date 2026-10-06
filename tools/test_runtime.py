"""Compile and run the production state machines without ESP-IDF or hardware."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
# The UI language is chosen at build time (src/i18n/Strings.h), so every suite
# runs once per language: the tests must not depend on either one's wording.
LANGUAGES = {"ja": [], "en": ["-DLAUNCHER_LANGUAGE_EN=1"]}

def main():
    compiler = shutil.which("g++")
    if not compiler:
        raise SystemExit("g++ is required (on Windows: MSYS2 UCRT64 GCC).")
    with tempfile.TemporaryDirectory(prefix="launcher-runtime-") as directory:
        for language, flags in LANGUAGES.items():
            for suite in ("runtime_tests", "ui_tests", "time_tests", "settings_tests",
                          "multifirm_tests", "stopwatch_tests", "timer_tests", "background_tests", "watchface_tests"):
                binary = Path(directory) / f"{suite}-{language}.exe"
                sources = [f"tests/{suite}.cpp", "src/input/InputController.cpp",
                           "src/host/ScreenManager.cpp", "src/host/HostRuntime.cpp", "src/ui/rendering/Element.cpp",
                           "src/ui/list/ListController.cpp", "src/features/launcher/LauncherController.cpp",
                           "src/services/TimeService.cpp", "src/features/home/HomeDataSource.cpp",
                           "src/storage/SettingsStore.cpp", "src/storage/WatchPreferences.cpp", "src/features/settings/SettingsScreen.cpp",
                           "src/features/external/ExternalAppScreen.cpp",
                           "src/services/StopwatchService.cpp", "src/services/TimerService.cpp", "src/features/timer/TimerScreen.cpp", "src/features/timer/TimerBackgroundInfo.cpp", "src/storage/TimerPreferences.cpp",
                           "src/features/stopwatch/StopwatchFormat.cpp", "src/features/stopwatch/StopwatchScreen.cpp",
                           "src/features/stopwatch/StopwatchBackgroundInfo.cpp",
                           "src/features/background/BackgroundInfoHub.cpp", "src/ui/graphics/MaskImage.cpp", "src/ui/graphics/VlwGlyphs.cpp",
                           # Stands in for src/assets/AppIcons.cpp, which links the embedded icons.
                           "tests/HostAppIcons.cpp"]
                subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                                "-finput-charset=UTF-8", "-fexec-charset=UTF-8", *flags,
                                "-I", str(ROOT / "src"), *[str(ROOT / s) for s in sources],
                                "-o", str(binary)], check=True)
                subprocess.run([str(binary)], check=True)

if __name__ == "__main__":
    main()
