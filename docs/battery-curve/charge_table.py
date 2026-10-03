"""Charging voltage -> charge, from one charge record: charging time stands in
for charge (the current held steady), with `offset_min` of charging before the
record began. Usage: python charge_table.py <csv> <offset_min>"""
import csv, sys
rows = [(int(r['t_s']), int(r['vbat_mv']), r['state']) for r in csv.DictReader(open(sys.argv[1]))]
offset = float(sys.argv[2]) * 60
ch = [0]
for (t0, v0, s0), (t1, v1, s1) in zip(rows, rows[1:]):
    ch.append(ch[-1] + (t1 - t0 if 'chg' in s0 and 'paused' not in s0 else 0))
done = next(ch[i] for i, r in enumerate(rows) if 'chg' not in r[2] and 'paused' not in r[2])
total = done + offset
charging = [(ch[i], v) for i, (t, v, s) in enumerate(rows) if 'chg' in s and 'paused' not in s]
print(f'charging {done/60:.1f} min + {offset/60:.0f} min before the record; start {charging[0][1]}mV = {offset/total*100:.1f}%')
for V in range(3450, 4201, 50):
    for (c0, v0), (c1, v1) in zip(charging, charging[1:]):
        if v0 < V <= v1:
            c = c0 + (c1 - c0) * (V - v0) / (v1 - v0) if c1 > c0 else c1
            print(V, f'{(c + offset) / total * 100:.1f}'); break
reach = next(c for c, v in charging if v >= 4200)
print(f'4200mV reached at {reach/60:.1f} min, charger done {(done-reach)/60:.1f} min later')
