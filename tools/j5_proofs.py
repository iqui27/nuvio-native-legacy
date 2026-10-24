#!/usr/bin/env python3
"""Assemble J5 native captures beside the retained mock/reference columns."""
import json
import os
import subprocess
from pathlib import Path

BASE = Path('/Volumes/ExternalSSD/tmp/rev/diretor21/21-j5')
OUT = BASE / 'r2'
os.environ['MAGICK_TEMPORARY_PATH'] = '/tmp'
TIMES = (0, 80, 160, 320)


def magick(*args):
    subprocess.run(['magick', *map(str, args)], check=True, capture_output=True)


def native(path):
    if path.exists():
        return path
    bmp = path.with_suffix('.bmp')
    if bmp.exists():
        magick(bmp, path)
    if not path.exists():
        raise FileNotFoundError(path)
    return path


def strip(paths, target):
    magick(*map(native, paths), '+append', target)


pngs = []
for screen, directory, prefix, still in (
    ('home21', 'home21', 'home21', 'home21-{scale}-{material}'),
    ('servers-detail', 'servers-detail', 'server', 'server-{scale}-{material}'),
    ('servers-sources', 'servers-sources', 'native', 'native-{scale}-{material}'),
    ('servers-overview', 'servers-overview', 'overview', 'overview-{scale}-{material}'),
    ('together', 'together', '', 'matrix-7-{scale}-{material}'),
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
            if scale == 100 and material == 'glass':
                magick(target, OUT / f'{screen}-tri.png')
    for mode in ('entry', 'focus'):
        if screen == 'together':
            paths = [frames / f'layer-7-100-glass-{mode}-{t:03}.png' for t in TIMES]
        else:
            paths = [frames / f'{prefix}-{mode}-{t:03}.png' for t in TIMES]
        strip(paths, OUT / f'{screen}-{mode}-anim.png')
    target = OUT / f'{screen}-anim.png'
    magick(OUT / f'{screen}-entry-anim.png', OUT / f'{screen}-focus-anim.png', '-append', target)
    pngs.append(str(target))

for layer in range(1, 8):
    for scale in (100, 85):
        for material in ('glass', 'solid'):
            rows = []
            for mode in ('entry', 'focus', 'exit', 'reverse'):
                target = OUT / f'together-layer-{layer}-{scale}-{material}-{mode}-anim.png'
                strip([OUT / 'together' / f'layer-{layer}-{scale}-{material}-{mode}-{t:03}.png' for t in TIMES], target)
                rows.append(target)
            target = OUT / f'together-layer-{layer}-{scale}-{material}-anim.png'
            magick(*rows, '-append', target)
            pngs.append(str(target))

(OUT / 'proofs.json').write_text(json.dumps({'pngs': pngs, 'times_ms': TIMES,
    'reference_limit': 'Retained round-one mock/reference columns; Juntos has no approved mock.'}, indent=2) + '\n')
print(f'{len(pngs)} proof images assembled from native frames')
