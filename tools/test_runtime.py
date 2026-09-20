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
        for suite in ("runtime_tests", "ui_tests"):
            binary = Path(directory) / (suite + ".exe")
            sources = [f"tests/{suite}.cpp", "src/input/InputController.cpp",
                       "src/app/ScreenManager.cpp", "src/app/AppRuntime.cpp", "src/ui/Element.cpp"]
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-finput-charset=UTF-8", "-fexec-charset=UTF-8",
                            "-I", str(ROOT / "src"), *[str(ROOT / s) for s in sources],
                            "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

if __name__ == "__main__":
    main()
