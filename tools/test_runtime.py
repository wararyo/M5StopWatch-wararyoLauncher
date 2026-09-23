"""Compile and run the production state machines without ESP-IDF or hardware."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    compiler = shutil.which("g++")
    if not compiler:
        raise SystemExit("g++ is required (on Windows: MSYS2 UCRT64 GCC).")
    with tempfile.TemporaryDirectory(prefix="launcher-runtime-") as directory:
        for suite in ("runtime_tests", "ui_tests", "time_tests", "settings_tests",
                      "multifirm_tests", "stopwatch_tests"):
            binary = Path(directory) / (suite + ".exe")
            sources = [f"tests/{suite}.cpp", "src/input/InputController.cpp",
                       "src/app/ScreenManager.cpp", "src/app/AppRuntime.cpp", "src/ui/rendering/Element.cpp",
                       "src/ui/list/ListController.cpp", "src/features/launcher/LauncherController.cpp",
                       "src/services/TimeService.cpp", "src/services/LauncherData.cpp",
                       "src/storage/SettingsStore.cpp", "src/features/settings/SettingsScreen.cpp",
                       "src/features/external/ExternalAppScreen.cpp",
                       "src/services/StopwatchService.cpp",
                       "src/features/stopwatch/StopwatchFormat.cpp", "src/features/stopwatch/StopwatchScreen.cpp"]
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-finput-charset=UTF-8", "-fexec-charset=UTF-8",
                            "-I", str(ROOT / "src"), *[str(ROOT / s) for s in sources],
                            "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

if __name__ == "__main__":
    main()
