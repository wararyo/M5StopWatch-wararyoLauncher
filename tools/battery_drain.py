"""Dump the battery-voltage record of the m5stopwatch-drain build (work 8-0).

Sends `O`, saves the DRAIN lines as CSV and prints the time spent in each
voltage band. Opens the port with DTR/RTS low, like capture_serial.py, so the
dump never resets the board (a reset would drop the 1-minute RAM record).

Usage: python tools/battery_drain.py COM11 docs/task8/<date>-drain-<n>.csv
"""
import csv
from pathlib import Path
import re
import sys
import time

try:
    import serial
except ImportError:  # pragma: no cover - environment hint only
    raise SystemExit("pyserial is required: use the MultiFirm tools venv "
                     "(.pio/libdeps/m5stopwatch/M5StopWatch-MultiFirm/tools/.venv)")

FIELD = re.compile(r"(\w+)=(\S+)")
# Bit order of the state byte, as defined in src/power/DrainLog.cpp.
STATE_BITS = ("lit", "chg", "usb")
# Upper edges of the bands compared between runs (docs/task8/plan.md).
BANDS = ((4100, 3900), (3900, 3700), (4100, 3700), (3700, 3600))


def fields(text):
    return {k: (int(v) if v.lstrip("-").isdigit() else v) for k, v in FIELD.findall(text)}


def describe(bits):
    return "+".join(n for i, n in enumerate(STATE_BITS) if bits >> i & 1) or "-"


def crossing(records, mv):
    """First time (s) the voltage is at or below mv, interpolated between samples."""
    for prev, cur in zip(records, records[1:]):
        if cur["vbat"] <= mv < prev["vbat"]:
            span = prev["vbat"] - cur["vbat"]
            return prev["t"] + (cur["t"] - prev["t"]) * (prev["vbat"] - mv) / span
    return records[0]["t"] if records and records[0]["vbat"] <= mv else None


def read_dump(port):
    device = serial.Serial()
    device.port, device.baudrate, device.timeout = port, 115200, 0.2
    device.dtr = False
    device.rts = False
    device.open()
    header, records, sleep = None, [], None
    with device:
        device.reset_input_buffer()
        device.write(b"O")
        deadline, buffer = time.time() + 20, b""
        while time.time() < deadline:
            buffer += device.read(4096)
            *lines, buffer = buffer.split(b"\n")
            for raw in lines:
                line = raw.decode("utf-8", "replace").strip()
                at = line.find("DRAIN ")
                if at < 0:
                    continue
                values = fields(line[at + 6:])
                if "source" in values:
                    header = values
                elif {"t", "vbat", "st"} <= values.keys():
                    records.append(values)
                elif "slept_s" in values:
                    sleep = values
                elif "n" in values:
                    values["sleep"] = sleep
                    return header, records, values
    raise SystemExit("No complete DRAIN dump within 20s (is this the m5stopwatch-drain build?)")


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    port, path = sys.argv[1], Path(sys.argv[2])
    header, records, end = read_dump(port)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as out:
        writer = csv.writer(out)
        writer.writerow(["t_s", "vbat_mv", "state"])
        for r in records:
            writer.writerow([r["t"], r["vbat"], describe(r["st"])])
    print(f"[drain] source={header.get('source')} interval={header.get('interval')} "
          f"active={header.get('active')} n={len(records)} -> {path}")
    if end.get("nvs_n") is not None:
        print(f"[drain] nvs copy holds {end['nvs_n']} entries")
    # Automatic light sleep during the run (work 8-5); RAM only, like the record.
    if end.get("sleep") and records:
        sleep, span = end["sleep"], max(records[-1]["t"], 1)
        print(f"[drain] light sleep: {sleep['count']} times, {sleep['slept_s']}s "
              f"({100 * sleep['slept_s'] / span:.1f}% of the recorded {span}s)")
    if not records:
        return
    # A failed PMIC read comes back as <= 0; it is not a voltage.
    valid = [r for r in records if r["vbat"] > 0]
    if len(valid) != len(records):
        print(f"[drain] ignored {len(records) - len(valid)} failed reads")
    first, last = valid[0], valid[-1]
    print(f"[drain] {first['vbat']}mV -> {last['vbat']}mV over {last['t'] / 3600:.2f}h "
          f"(last sample at t={last['t']}s)")
    for high, low in BANDS:
        a, b = crossing(valid, high), crossing(valid, low)
        span = "not reached" if a is None or b is None else f"{(b - a) / 3600:.2f}h"
        print(f"[drain] {high / 1000:.1f}V -> {low / 1000:.1f}V: {span}")
    states = sorted({r["st"] for r in valid})
    print("[drain] states seen: " + ", ".join(describe(s) for s in states))


if __name__ == "__main__":
    main()
