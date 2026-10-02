import csv, sys
sys.path.insert(0,'tools')
from battery_charge import curve, percent
rows=[(int(r['t_s']),int(r['vbat_mv']),r['state']) for r in csv.DictReader(open('docs/battery-curve/20261002-charge-1-check6.csv'))]
pts=curve()
# charging time: accumulate dt between consecutive samples only when the earlier one is not paused
# and the later one is not paused (a rest spans before->first paused->...->last paused)
ch=[0]; 
for (t0,v0,s0),(t1,v1,s1) in zip(rows,rows[1:]):
    charging = ('paused' not in s0) and ('chg' in s0)
    ch.append(ch[-1]+(t1-t0 if charging else 0))
# rests: last paused sample of each run
end_cc=None
for i,(t,v,s) in enumerate(rows):
    if 'chg' not in s and 'paused' not in s: end_cc=ch[i]; break
T=end_cc
print(f'total charging time until charger stopped: {T/60:.1f} min; record start {rows[0]}')
# IR drop per rest and rest table
print('rest#  t_min  chg_time_min  time_soc%  rested_mV  table_soc%  diff  chg_mV_before  drop_mV')
k=0
for i,(t,v,s) in enumerate(rows):
    if 'paused' in s and (i+1==len(rows) or 'paused' not in rows[i+1][2]):
        # find before sample
        j=i
        while 'paused' in rows[j][2]: j-=1
        k+=1; ts=ch[i]/T*100; tb=percent(pts,v)
        print(f'{k:5d} {t/60:6.1f} {ch[i]/60:12.1f} {ts:9.1f} {v:10d} {tb:10.1f} {tb-ts:+5.1f} {rows[j][1]:13d} {rows[j][1]-v:7d}')
# charging-voltage -> time soc (1-min charging samples)
print('\ncharging V -> time-based SoC (first crossing)')
for V in range(3450,4201,50):
    for i,(t,v,s) in enumerate(rows):
        if 'paused' not in s and 'chg' in s and v>=V:
            print(V, f'{ch[i]/T*100:5.1f}%', f'(t={t/60:.1f}min)'); break
