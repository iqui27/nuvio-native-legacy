#!/usr/bin/env python3
"""Sample rendered RGB, including every frame around the renderer hand-off."""
import csv
import pathlib
import struct
import subprocess
import sys


def samples(directory, pattern='c-frame-*.png'):
    # Average 16x16 pixels under the left scrim, away from moving edges.
    paths = sorted(directory.glob(pattern))
    raw = subprocess.check_output([
        'magick', *map(str, paths), '-crop', '16x16+240+540', '+repage',
        '-scale', '1x1!', '-depth', '8', 'rgb:-'])
    return [tuple(raw[i:i+3]) for i in range(0, len(raw), 3)]


def endpoint(path):
    raw = path.read_bytes()
    off = struct.unpack_from('<I', raw, 10)[0]
    w, h = struct.unpack_from('<ii', raw, 18)
    assert (w, h) == (1920, 1080)
    return raw, off


failures = []
root = pathlib.Path(sys.argv[1])
for directory in sorted(root.glob('L*-*')):
    rows = list(csv.DictReader((directory / 'c-frames.csv').open()))
    assert len(rows) == 100, f'{directory}: incomplete frame sequence'
    colors = samples(directory)
    # All frames after the initial growth, not a sparse screenshot selection.
    jumps = [max(abs(a-b) for a, b in zip(colors[i-1], colors[i]))
             for i in range(30, len(colors))]
    peak = max(jumps)
    print(f'{directory.name}: every-frame veil max channel step={peak}/255')
    if peak > 5:
        failures.append(f'{directory.name}: one-frame colour/opacity jump {peak}/255')
    if list(directory.glob('c-late-*.png')):
        late = samples(directory, 'c-late-*.png')
        assert len(late) == 24
        first = max(abs(a-b) for a,b in zip(late[3],late[4]))
        peak_late = max(max(abs(a-b) for a,b in zip(x,y)) for x,y in zip(late,late[1:]))
        settled = max(abs(a-b) for a,b in zip(late[-1],colors[-1]))
        print(f'{directory.name}: late-art first={first} max-step={peak_late} settled-delta={settled}')
        if first > 1 or peak_late > 12 or settled > 2:
            failures.append(f'{directory.name}: late-art cut or wrong settled scrim')
        if not directory.name.endswith('-frost') and max(late[0]) < 20:
            failures.append(f'{directory.name}: missing artwork fell back to black instead of tint')
    if not (directory / 'c-endpoint-0.bmp').exists():
        continue
    a, off = endpoint(directory / 'c-endpoint-0.bmp')
    b, other = endpoint(directory / 'c-endpoint-1.bmp')
    total = count = bad = 0
    for y in range(8, 1072, 4):
        for x in range(8, 1912, 4):
            p = off + (y*1920+x)*4
            q = other + (y*1920+x)*4
            for channel in range(3):
                delta = abs(a[p+channel]-b[q+channel])
                total += delta
                count += 1
                bad += delta > 3
    mean = total/count
    print(f'{directory.name}: hand-off mean={mean:.3f}/255 channels >3={bad/count:.3%}')
    if mean > 1 or bad/count > .05:
        failures.append(f'{directory.name}: discontinuous renderer endpoint')
if failures:
    raise SystemExit('FAIL: ' + '; '.join(failures))
assert len(list(root.glob('L*-*'))) == 9, 'missing layout/material coverage'
print('PASS: three layouts, Glass off/Frost/Glass; no black jump')
