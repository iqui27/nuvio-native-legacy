#!/usr/bin/env python3
"""Capture native detail pages and compare approved mock crops, offline at 1080p."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = Path('/Volumes/ExternalSSD/tmp/rev/detalhe21-layout-png')
REF = Path('/Volumes/ExternalSSD/tmp/detalhe21/v2')


def run(*args, **kwargs):
    subprocess.run([str(a) for a in args], check=True, cwd=ROOT, **kwargs)


def crop(src, box, dest):
    run('magick', src, '-crop', box, '+repage', dest)


OUT.mkdir(parents=True, exist_ok=True)
for mode, name in enumerate(('pronta', 'pessoal')):
    work = OUT / name
    work.mkdir(exist_ok=True)
    env = dict(os.environ, OBJ='/tmp/nv21-resto-shot-obj', NUVIO_SHOT_IDIOMA='0',
               NUVIO_SHOT_REFERENCE=str(REF / 'assets'), NUVIO_SHOT_SOURCE='1',
               NUVIO_SHOT_AMIGOS='1', NUVIO_SHOT_TOPO=str(mode),
               NUVIO_SHOT_TIME='1792842060', TZ='America/Sao_Paulo')  # Saved reference: 08:41.
    with (work / 'capture.log').open('w') as log:
        run('bash', 'tests/detalhe_glass_shot.sh', work, 'full', env=env,
            stdout=log, stderr=subprocess.STDOUT)
    for kind in ('movie', 'series'):
        height = int((work / f'{kind}-height.txt').read_text())
        tiles = []
        for bmp in sorted(work.glob(f'{kind}-full-*.bmp')):
            y = int(bmp.stem.rsplit('-', 1)[1])
            tile = bmp.with_suffix('.png')
            crop(bmp, f'1920x{min(900, height-y)}+0+0', tile)
            tiles.append(tile)
        run('magick', *tiles, '-append', OUT / f'{kind}-{name}.png')

# Keep a close crop of the native ratings row alongside both full pages.
positions = {int(r): int(y) for r, y, _ in
             (line.split() for line in (OUT / 'pronta' / 'movie-sections.txt').read_text().splitlines())}
crop(OUT / 'movie-pronta.png', f'1728x220+96+{positions[8]}', OUT / 'notas-media.png')

# Runtime section positions come from the native layout, never inferred from pixels.
# Each comparison keeps the mock's width and scale: mock left, app right.
comparisons = [
    ('14-hero', 'movie', None, 0, 940),
    ('15-ratings', 'movie', 8, 1000, 220),
    ('16-trailers', 'movie', 10, 1330, 330),
    ('17-cast', 'movie', 3, 1740, 440),
    ('18-collection', 'movie', 11, 2230, 350),
    ('19-related', 'movie', 4, 2600, 440),
    ('20-comments', 'movie', 12, 3080, 280),
    ('21-footer', 'movie', 14, 3400, 320),
    ('22-episodes', 'series', 0, 1000, 660),
    ('23-progress', 'series', 16, 1720, 340),
]
for number, kind, section, mock_y, height in comparisons:
    positions = {int(r): int(y) for r, y, _ in
                 (line.split() for line in (OUT / 'pronta' / f'{kind}-sections.txt').read_text().splitlines())}
    app_y = positions[section] if section is not None else 0
    mock = OUT / 'pronta' / 'mock.png'
    app = OUT / 'pronta' / 'app.png'
    crop(REF / 'pagina-completa-pronta.png',
         f'1920x{height}+{1980 if kind == "series" else 0}+{mock_y}', mock)
    crop(OUT / f'{kind}-pronta.png', f'1920x{height}+0+{app_y}', app)
    run('magick', mock, app, '+append', OUT / f'{number}.png')
print(OUT)
