"""Record the device serial log for a fixed duration (work 7 measurements).

The console is USB Serial JTAG, so opening the port does not reboot the board.
Pass `reset` when the startup log is wanted; leave it off to keep a run going,
which is what the long stopwatch measurement needs.

Usage: python tools/capture_serial.py COM11 3900 docs/task7/<name>.log [reset]
"""
from pathlib import Path
import sys
import time

try:
    import serial
except ImportError:  # pragma: no cover - environment hint only
    raise SystemExit("pyserial is required: use the MultiFirm tools venv "
                     "(.pio/libdeps/m5stopwatch/M5StopWatch-MultiFirm/tools/.venv)")


def main():
    if len(sys.argv) < 4:
        raise SystemExit(__doc__)
    port, seconds, path = sys.argv[1], float(sys.argv[2]), Path(sys.argv[3])
    reset = len(sys.argv) > 4 and sys.argv[4] == "reset"
    path.parent.mkdir(parents=True, exist_ok=True)
    # USB Serial JTAG reboots on the DTR/RTS pattern esptool uses, and pyserial
    # asserts both when it opens a port. Without `reset` that is a trap: the
    # lines drop again on close and the device reboots after the recording has
    # stopped, which is how the first long stopwatch run lost its elapsed value.
    # So a plain capture decides the state before opening and never moves a
    # line. A `reset` capture wants the reboot and lets the open assert them,
    # which is also what keeps the port alive across the startup log.
    device = serial.Serial()
    device.port = port
    device.baudrate = 115200
    device.timeout = 0.2
    if not reset:
        device.dtr = False
        device.rts = False
    device.open()
    with device, path.open("ab") as out:
        if reset:
            device.dtr = False
            device.rts = True  # EN low
            time.sleep(0.2)
            device.rts = False
            time.sleep(0.1)
            device.reset_input_buffer()
        deadline = time.time() + seconds
        while time.time() < deadline:
            data = device.read(4096)
            if not data:
                continue
            out.write(data)
            out.flush()
            sys.stdout.write(data.decode("utf-8", "replace"))
            sys.stdout.flush()
    print(f"\n[capture] {seconds:.0f}s -> {path}")


if __name__ == "__main__":
    main()
