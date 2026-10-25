"""Rebuild full-size comparisons for the three J3 P1 routes from native stills."""
from pathlib import Path
import subprocess

ROOT = Path('/Volumes/ExternalSSD/tmp/rev/diretor21/21-j3/r6')
# Preserved middle panel from the preceding labelled 2.0.8 comparison.
REFERENCE = ROOT / 'reference-208.png'
FONT = 'deploy/app/fonts/InterDisplay-Regular.ttf'
ROUTES = (
    ('conectar-debrid', 'guide', '-app', '/Volumes/ExternalSSD/tmp/addon-guia-mock/conectar-debrid.png'),
    ('aio-setup-servicos', 'aio', '', '/Volumes/ExternalSSD/tmp/aiostreams-mock/aio-setup-servicos.png'),
    ('aio-filtros', 'aio', '', '/Volumes/ExternalSSD/tmp/aiostreams-mock/aio-filtros.png'),
)


def magick(*args):
    subprocess.run(['magick', '-limit', 'thread', '1', *map(str, args)], check=True)


def labelled(source, label):
    return ['(', source, '-background', '#121316', '-fill', '#f3f2ef',
            '-font', FONT, '-pointsize', '28', '-gravity', 'North',
            '-splice', '0x44', '-annotate', '+0+6', label, ')']


for route, folder, suffix, mock in ROUTES:
    owner = Path('src/addonguia.c' if folder == 'guide' else 'src/aiostreamsui.c')
    for scale in (100, 85):
        for material in ('solid', 'glass'):
            stem = f'{route}-{scale}-{material}'
            app = ROOT / folder / f'{stem}{suffix}.png'
            assert app.stat().st_mtime >= owner.stat().st_mtime, f'Stale capture: {app}'
            trio = ROOT / f'{stem}-tri.png'
            diff = ROOT / f'{stem}-overlay-diff.png'
            magick(*labelled(mock, 'Mock | pt-BR'),
                    *labelled(REFERENCE, '2.0.8 | reference'),
                    *labelled(app, f'App | pt-BR | {scale}% | {material}'), '+append', trio)
            magick('(', mock, app, '-evaluate-sequence', 'Mean', ')',
                    '(', mock, app, '-compose', 'Difference', '-composite', ')',
                    '+append', diff)
            print(trio)
            print(diff)
print('12 full-size labelled trios and 12 overlay/diffs rebuilt.')
