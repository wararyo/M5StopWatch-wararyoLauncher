"""Read-only checks of a completed product build; never connects to a device."""
import hashlib
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

from upload_guard import check_size

ROOT = Path(__file__).resolve().parents[1]
PIN = "f5a8fd88f50dfb50c6e6e1e82bccc491a936bd47"
BUILD = ROOT / ".pio/build/m5stopwatch"
MULTIFIRM = ROOT / ".pio/libdeps/m5stopwatch/M5StopWatch-MultiFirm"
EXPECTED = {
    "IDF_TARGET": "esp32s3",
    "ESPTOOLPY_FLASHSIZE": "16MB",
    "ESPTOOLPY_FLASHMODE": "dio",
    "ESPTOOLPY_FLASHFREQ": "80m",
    "PARTITION_TABLE_OFFSET": 0x8000,
    "SPIRAM": True,
    "SPIRAM_MODE_OCT": True,
    "SPIRAM_SPEED": 80,
    "ESP_DEFAULT_CPU_FREQ_MHZ": 240,
    "ESP_MAIN_TASK_AFFINITY_CPU1": True,
    "ESP_MAIN_TASK_STACK_SIZE": 8192,
    "FREERTOS_HZ": 1000,
    "ESP_TASK_WDT_EN": True,
    "ESP_TASK_WDT_INIT": True,
    "ESP_TASK_WDT_TIMEOUT_S": 5,
    "ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0": True,
    "ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1": True,
    "ESP_CONSOLE_USB_SERIAL_JTAG": True,
    "ESP32S3_INSTRUCTION_CACHE_SIZE": 16384,
    "ESP32S3_DATA_CACHE_SIZE": 65536,
    "ESP32S3_DATA_CACHE_LINE_SIZE": 64,
    "PM_ENABLE": False,
    "FREERTOS_USE_TICKLESS_IDLE": False,
    "BOOTLOADER_APP_ROLLBACK_ENABLE": False,
    "SECURE_BOOT": False,
    "SECURE_FLASH_ENC_ENABLED": False,
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    global BUILD, MULTIFIRM
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--environment",
                        choices=("m5stopwatch", "m5stopwatch-diagnostics",
                                 "m5stopwatch-measure", "m5stopwatch-render-check",
                                 "m5stopwatch-drain"),
                        default="m5stopwatch")
    environment = parser.parse_args().environment
    BUILD = ROOT / ".pio/build" / environment
    MULTIFIRM = ROOT / ".pio/libdeps" / environment / "M5StopWatch-MultiFirm"
    config = json.loads((BUILD / "config/sdkconfig.json").read_text())
    for key, value in EXPECTED.items():
        # Kconfig omits some disabled symbols whose dependencies are disabled.
        actual = config.get(key, False if value is False else None)
        require(actual == value,
                f"{key}: expected {value!r}, got {config.get(key)!r}")
    print("[OK] generated SDK configuration")
    revision = subprocess.check_output([
        "git", "-c", f"safe.directory={MULTIFIRM.as_posix()}",
        "-C", str(MULTIFIRM), "rev-parse", "HEAD"], text=True).strip()
    require(revision == PIN, f"MultiFirm revision mismatch: {revision}")
    print(f"[OK] MultiFirm {revision}")

    description = json.loads((BUILD / "project_description.json").read_text())
    generator = Path(description["idf_path"]) / "components/partition_table/gen_esp32part.py"
    spec = importlib.util.spec_from_file_location("gen_esp32part", generator)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    canonical = module.PartitionTable.from_csv(
        (MULTIFIRM / "docs/partitions.multifirm.csv").read_text()).to_binary()
    product = module.PartitionTable.from_csv((ROOT / "partitions.csv").read_text()).to_binary()
    require(product == canonical, "Product CSV differs from pinned MultiFirm layout")
    require((BUILD / "partitions.bin").read_bytes() == canonical,
            "Generated partition binary differs from pinned MultiFirm layout")
    print("[OK] all partition entries and generated binary match MultiFirm")

    image = BUILD / "firmware.bin"
    check_size(image)
    image_bytes = image.read_bytes()
    require((b"[RuntimeDiag] enabled" in image_bytes) == (environment == "m5stopwatch-diagnostics"),
            "Runtime overload diagnostics do not match the selected environment")
    require((b"[RenderDiag] synthetic" in image_bytes) == (environment == "m5stopwatch-render-check"),
            "Synthetic clock data do not match the selected environment")
    require((b"[Verify] checks=" in image_bytes) == (environment == "m5stopwatch-render-check"),
            "Pixel checks do not match the selected environment")
    # The product build carries no measurement instrument at all: this is the
    # check behind "remove the verification-only buffers" in work 7.
    require((b"[RenderDiag] window_us=" in image_bytes) ==
            (environment in ("m5stopwatch-measure", "m5stopwatch-render-check")),
            "Frame metrics do not match the selected environment")
    require((b"[Drain] enabled" in image_bytes) == (environment == "m5stopwatch-drain"),
            "Battery drain record does not match the selected environment")
    print("[OK] overload diagnostics / synthetic clock / pixel checks / frame metrics / "
          "drain record isolated by environment")
    print(f"[OK] firmware SHA-256={hashlib.sha256(image.read_bytes()).hexdigest()}")
    print(f"[Build] project={description['project_name']} "
          f"version={description['project_version']} IDF={description['git_revision']}")
    # Use the same pinned tool for both image validation and the non-device preview.
    tool = MULTIFIRM / "tools/multifirm.py"
    for args in (["inspect", str(image), "--role", "host"],
                 ["install-host", str(image)]):
        subprocess.run([sys.executable, str(tool), *args], cwd=ROOT, check=True)
    print("[OK] host image inspection and install-host preview (no device access)")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        sys.exit(f"Build verification failed: {error}")
