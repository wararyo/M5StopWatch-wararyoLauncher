"""Dump the charging record of the m5stopwatch-charge build.

Sends `C`, saves the CHARGE lines as CSV and prints one row per rest: the
charging voltage right before it, the rested voltage, and the charge that the
rested voltage means on the discharge curve (src/power/BatteryCurve.h).
Opens the port like battery_drain.py, so the dump never resets the board.

Usage: python tools/battery_charge.py COM11 docs/battery-curve/<date>-charge-<n>.csv
       python tools/battery_charge.py docs/battery-curve/<date>-charge-<n>.csv   (summary only)
"""
import csv
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
# Bit order of the state byte, as defined in src/power/ChargeLog.cpp.
STATE_BITS = ("paused", "chg", "lit", "unplugged")


def describe(bits):
    return "+".join(n for i, n in enumerate(STATE_BITS) if bits >> i & 1) or "-"


def curve():
    text = (ROOT / "src/power/BatteryCurve.h").read_text(encoding="utf-8")
    return [(int(mv), int(p)) for mv, p in re.findall(r"\{(\d{4}),(\d+)\}", text)]


def percent(points, mv):
    """Same interpolation as batteryPercentFromMv, without the rounding."""
    if mv >= points[0][0]:
        return float(points[0][1])
    for (hv, hp), (lv, lp) in zip(points, points[1:]):
        if mv >= lv:
            return lp + (hp - lp) * (mv - lv) / (hv - lv)
    return float(points[-1][1])


def rests(records):
    """Each run of paused samples, with the charging sample just before it."""
    runs, current, before = [], None, None
    for r in records:
        if r["paused"]:
            if current is None:
                current = {"before": before, "samples": []}
                runs.append(current)
            current["samples"].append(r)
        else:
            current, before = None, r
    return runs


def summarize(records):
    points = curve()
    valid = [r for r in records if r["vbat"] > 0]
    if len(valid) != len(records):
        print(f"[charge] ignored {len(records) - len(valid)} failed reads")
    if not valid:
        return
    print(f"[charge] {valid[0]['vbat']}mV -> {valid[-1]['vbat']}mV over {valid[-1]['t'] / 3600:.2f}h")
    for r in valid:
        if r["unplugged"]:
            print(f"[charge] USB power lost at t={r['t']}s")
    idle = next((r for r in valid if not r["paused"] and not r["chg"] and not r["unplugged"] and r["t"] > 0), None)
    if idle:
        print(f"[charge] charger first reported not charging at t={idle['t']}s")
    print("[charge]   t_min  charging_mV  +10s_mV  +60s_mV  rested_mV  last60s_mV  rested_%")
    for run in rests(valid):
        samples, before, end = run["samples"], run["before"], run["samples"][-1]
        # Charging stops right after the "before" sample; offsets count from it.
        start = before["t"] if before else samples[0]["t"]

        def at(offset):
            hit = next((s for s in samples if s["t"] - start >= offset), None)
            return hit["vbat"] if hit else None
        minute_ago = next((s for s in reversed(samples) if end["t"] - s["t"] >= 60), None)
        drift = end["vbat"] - minute_ago["vbat"] if minute_ago else None
        print(f"[charge] {start / 60:7.1f}  {before['vbat'] if before else '-':>11}  "
              f"{at(10) or '-':>7}  {at(60) or '-':>7}  {end['vbat']:>9}  "
              f"{drift if drift is not None else '-':>10}  {percent(points, end['vbat']):8.1f}")


def main():
    args = sys.argv[1:]
    if len(args) == 1 and args[0].lower().endswith(".csv"):
        with Path(args[0]).open(newline="") as source:
            records = [{"t": int(r["t_s"]), "vbat": int(r["vbat_mv"]),
                        "paused": "paused" in r["state"], "chg": "chg" in r["state"],
                        "unplugged": "unplugged" in r["state"]}
                       for r in csv.DictReader(source)]
        summarize(records)
        return
    if len(args) != 2:
        raise SystemExit(__doc__)
    from battery_drain import read_dump  # Needs pyserial; the summary does not.
    port, path = args
    header, raw, end = read_dump(port, b"C", "CHARGE")
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as out:
        writer = csv.writer(out)
        writer.writerow(["t_s", "vbat_mv", "state"])
        for r in raw:
            writer.writerow([r["t"], r["vbat"], describe(r["st"])])
    print(f"[charge] active={header.get('active')} phase={header.get('phase')} "
          f"overflow={header.get('overflow')} n={len(raw)} -> {path}")
    summarize([{"t": r["t"], "vbat": r["vbat"], "paused": bool(r["st"] & 1), "chg": bool(r["st"] & 2),
                "unplugged": bool(r["st"] & 8)}
               for r in raw])


if __name__ == "__main__":
    main()
