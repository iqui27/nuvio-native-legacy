#!/usr/bin/env python3
"""Assemble J5 native captures beside the retained mock/reference columns."""
import json
import os
import subprocess
import hashlib
import shutil
from pathlib import Path

BASE = Path('/Volumes/ExternalSSD/tmp/rev/diretor21/21-j5')
OUT = BASE / 'r3'
os.environ['MAGICK_TEMPORARY_PATH'] = os.environ.get('TMPDIR', '/Volumes/ExternalSSD/tmp')
TIMES = (0, 80, 160, 320)
CLOCK_TIMES = (0, 50, 100, 150, 250, 400)
sources = []


def magick(*args):
    subprocess.run(['magick', *map(str, args)], check=True, capture_output=True)


def native(path):
    bmp = path.with_suffix('.bmp')
    if bmp.exists() and (not path.exists() or bmp.stat().st_mtime_ns > path.stat().st_mtime_ns):
        magick(bmp, path)
    if not path.exists():
        raise FileNotFoundError(path)
    if not path.resolve().is_relative_to(OUT.resolve()):
        raise ValueError(f'Native frame outside fresh capture directory: {path}')
    sources.append({'path': str(path), 'mtime_ns': path.stat().st_mtime_ns,
                    'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    return path


def strip(paths, target):
    magick(*map(native, paths), '+append', target)


def clock_strip(paths, target):
    # Same six-frame, 3x2 composition as the official native sheet reference.
    rows = []
    for start in (0, 3):
        row = target.with_name(target.stem + f'-row{start}.png')
        args = []
        for t, path in zip(CLOCK_TIMES[start:start + 3], paths[start:start + 3]):
            args += ['(', native(path), '-resize', '640x360', '-background', '#121316',
                     '-fill', 'white', '-font', 'Helvetica', '-pointsize', '18',
                     '-gravity', 'southwest', '-splice', '0x28', '-annotate', '+12+4', f'{t} ms', ')']
        magick(*args, '+append', row)
        rows.append(row)
    magick(*rows, '-append', target)
    for row in rows:
        row.unlink()


pngs = []
for screen, directory, prefix, still in (
    ('home21', 'home21', 'home21', 'home21-{scale}-{material}'),
    ('servers-detail', 'servers-detail', 'server', 'server-{scale}-{material}'),
    ('servers-sources', 'servers-sources', 'native', 'native-{scale}-{material}'),
    ('servers-overview', 'servers-overview', 'overview', 'overview-{scale}-{material}'),
    ('together', 'together-current', '', 'matrix-3-{scale}-{material}'),
):
    frames = OUT / directory
    for scale in (100, 85):
        for material in ('glass', 'solid'):
            target = OUT / f'{screen}-{scale}-{material}-tri.png'
            reference = OUT / f'{screen}-reference.png'
            magick(BASE / f'{screen}-{scale}-{material}-tri.png',
                   '-crop', '3840x1080+0+0', '+repage', reference)
            after = native(frames / (still.format(scale=scale, material=material) + '.png'))
            magick(reference, after, '+append', target)
            pngs.append(str(target))
            shutil.copy2(target, BASE / target.name)
            if scale == 100 and material == 'glass':
                shutil.copy2(target, OUT / f'{screen}-tri.png')
                shutil.copy2(target, BASE / f'{screen}-tri.png')
    for mode in ('entry', 'focus'):
        paths = [frames / f'{prefix + "-" if prefix else ""}{mode}-{t:03}.png' for t in TIMES]
        strip(paths, OUT / f'{screen}-{mode}-anim.png')
    target = OUT / f'{screen}-anim.png'
    magick(OUT / f'{screen}-entry-anim.png', OUT / f'{screen}-focus-anim.png', '-append', target)
    pngs.append(str(target))
    shutil.copy2(target, BASE / target.name)

for layer in range(1, 8):
    for scale in (100, 85):
        for material in ('glass', 'solid'):
            target = OUT / f'together-layer-{layer}-{scale}-{material}-tri.png'
            magick(OUT / 'together-reference.png',
                   native(OUT / 'together-current' / f'matrix-{layer}-{scale}-{material}.png'), '+append', target)
            pngs.append(str(target))
            rows = []
            for mode in ('entry', 'focus', 'exit', 'reverse'):
                target = OUT / f'together-layer-{layer}-{scale}-{material}-{mode}-anim.png'
                clock_strip([OUT / 'together-current' / f'layer-{layer}-{scale}-{material}-{mode}-{t:03}.png' for t in CLOCK_TIMES], target)
                rows.append(target)
            target = OUT / f'together-layer-{layer}-{scale}-{material}-anim.png'
            magick(*rows, '-append', target)
            pngs.append(str(target))

            if layer != 3:
                for mode in ('entry', 'focus', 'exit'):
                    target = OUT / f'together-clock-{layer}-{scale}-{material}-{mode}-compare.png'
                    magick('/Volumes/ExternalSSD/tmp/rev/linguagem/ref/sheet-open-strip.png',
                           OUT / f'together-layer-{layer}-{scale}-{material}-{mode}-anim.png', '+append', target)
                    pngs.append(str(target))

(OUT / 'proofs.json').write_text(json.dumps({'pngs': pngs, 'times_ms': TIMES,
    'clock_times_ms': CLOCK_TIMES, 'native_sources': sources,
    'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
    'reference_limit': 'Retained mock/reference columns only; every app column is recaptured. Juntos has no approved mock.'}, indent=2) + '\n')
print(f'{len(pngs)} proof images assembled from native frames')
