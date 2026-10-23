import csv
import statistics
import sys

with open(sys.argv[1]) as f:
    rows = list(csv.DictReader(f))
for key in rows[0]:
    if key == 'frame':
        continue
    values = sorted(float(r[key]) for r in rows)
    print(f'{key}: median={statistics.median(values):.6f} p95={values[int(len(values)*.95)]:.6f} total={sum(values):.3f}')
