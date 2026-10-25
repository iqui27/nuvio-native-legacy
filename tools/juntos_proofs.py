#!/usr/bin/env python3
"""Assemble and verify lobby/sheet proofs without mixing other J5 jobs."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

LOBBY = '--lobby' in sys.argv
ROOT = Path('/Volumes/ExternalSSD/tmp/rev/diretor21/21-j5') / ('r6-lobby' if LOBBY else 'r5-together')
NATIVE = ROOT / 'native'
MOCK = ROOT.parent / 'r3/together-layer-1-85-glass-tri.png'
REFERENCE = Path('/Volumes/ExternalSSD/tmp/rev/linguagem/ref/sheet-open-strip.png')
TIMES = (0, 50, 100, 150, 250, 400)
SHEETS = () if LOBBY else (1, 2, 4, 5, 6, 7)
CONFIGS = ('100-glass', '100-solid', '85-glass', '85-solid')
os.environ['MAGICK_THREAD_LIMIT'] = '1'
os.environ['MAGICK_TEMPORARY_PATH'] = str(ROOT)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def magick(*args):
    return subprocess.check_output(['magick', *map(str, args)])


def pixels(path, crop=None):
    args = [path]
    if crop:
        args += ['-crop', crop, '+repage']
    return hashlib.sha256(magick(*args, '-depth', '8', 'rgba:-')).hexdigest()


def expected():
    names = {f'matrix-3-{scale}-{material}' for scale in (85, 90, 100, 110, 120, 130)
             for material in ('glass', 'solid')}
    names |= {f'matrix-{layer}-{config}' for layer in SHEETS for config in CONFIGS}
    names |= {f'{mode}-{t:03}' for mode in ('entry', 'focus') for t in (0, 80, 160, 320)}
    names |= {f'layer-{layer}-{config}-{mode}-{t:03}' for layer in SHEETS
              for config in CONFIGS for mode in ('entry', 'focus', 'exit', 'reverse') for t in TIMES}
    return {NATIVE / f'{name}.bmp' for name in names}


tracked = [Path(p) for p in ('src/juntosui.c', 'src/gfx.c', 'src/text.c', 'src/ilha.c',
                           'src/plrilha.c', 'src/tela21.h', 'tests/juntos_shot.c', __file__)]
if LOBBY:
    tracked.append(Path('tests/juntos_lobby.sh'))
manifest_path = ROOT / 'proofs.json'
if '--check' in sys.argv:
    manifest = json.loads(manifest_path.read_text())
    assert set(manifest['native']) == {str(p) for p in expected()}
    for group in ('native', 'artifacts', 'code'):
        for name, sha in manifest[group].items():
            assert digest(Path(name)) == sha, name
    code_time = max(p.stat().st_mtime_ns for p in tracked if p.suffix == '.c')
    assert all(p.stat().st_mtime_ns > code_time for p in expected())
    for config in CONFIGS:
        assert pixels(ROOT / f'lobby-{config}-tri.png', '1920x1080+3840+44') == pixels(NATIVE / f'matrix-3-{config}.bmp')
    if LOBBY:
        # These physical regions must not move or grow with reading size.
        for scale in (85, 90, 100, 110, 120, 130):
            frame = NATIVE / f'matrix-3-{scale}-solid.bmp'
            for crop in ('300x250+1100+220', '20x20+390+590', '31x31+212+127'):
                result = subprocess.run(['magick', 'compare', '-metric', 'RMSE',
                    '(', str(NATIVE / 'matrix-3-85-solid.bmp'), '-crop', crop, '+repage', ')',
                    '(', str(frame), '-crop', crop, '+repage', ')', 'null:'], capture_output=True, text=True)
                assert result.returncode in (0, 1), result.stderr
                assert float(result.stderr.split('(')[1].split(')')[0]) < .01, (scale, crop, result.stderr)
    print(f'PASS: {len(expected())} fresh native frames; four exact lobby columns; artifact/code hashes match')
    sys.exit(0)

assert expected() == set(NATIVE.glob('*.bmp')), 'Incomplete or mixed capture run'
artifacts = []

def save(name, *args):
    target = ROOT / f'{name}.png'
    magick(*args, target)
    artifacts.append(target)
    return target


for config in CONFIGS:
    save(f'lobby-{config}-tri', MOCK, '-crop', '3840x1080+0+0', '+repage',
         NATIVE / f'matrix-3-{config}.bmp', '+append', '-background', '#121316',
         '-fill', 'white', '-font', '/System/Library/Fonts/Helvetica.ttc', '-pointsize', '26',
         '-gravity', 'northwest', '-splice', '0x44', '-annotate', '+20+8', 'Mock (pt-BR)',
         '-annotate', '+1940+8', '2.0.8 reference', '-annotate', '+3860+8', f'App (pt-BR) | {config}')
    save(f'lobby-{config}-overlay-diff', '(', MOCK, '-crop', '1920x1080+0+0', '+repage', ')',
         NATIVE / f'matrix-3-{config}.bmp', '-compose', 'difference', '-composite')

args = []
for scale in (85, 90, 100, 110, 120, 130):
    args += ['(']
    for material in ('glass', 'solid'):
        args += ['(', NATIVE / f'matrix-3-{scale}-{material}.bmp', '-resize', '960x540', ')']
    args += ['+append', ')']
save('lobby-scale-matrix', *args, '-append')
args = []
for mode in ('entry', 'focus'):
    args += ['(']
    for t in (0, 80, 160, 320):
        args += ['(', NATIVE / f'{mode}-{t:03}.bmp', '-resize', '480x270', ')']
    args += ['+append', ')']
save('lobby-entry-focus', *args, '-append')

for layer in SHEETS:
    for config in CONFIGS:
        args = []
        for mode in ('entry', 'focus', 'exit', 'reverse'):
            args += ['(']
            for t in TIMES:
                args += ['(', NATIVE / f'layer-{layer}-{config}-{mode}-{t:03}.bmp', '-resize', '640x360',
                         '-background', '#121316', '-fill', 'white', '-font', '/System/Library/Fonts/Helvetica.ttc',
                         '-pointsize', '20', '-gravity', 'northwest', '-splice', '0x30', '-annotate', '+12+4',
                         f'Sheet {layer} | {config} | {mode} | {t} ms', ')']
            args += ['+append', ')']
        save(f'sheet-{layer}-{config}-motion', *args, '-append')
    save(f'sheet-{layer}-reference-compare', REFERENCE,
         '(', ROOT / f'sheet-{layer}-85-glass-motion.png', '-crop', '3840x390+0+0', '+repage',
         '-resize', '1920x195', ')', '-append')

manifest = {'native': {str(p): digest(p) for p in sorted(expected())},
            'artifacts': {str(p): digest(p) for p in artifacts},
            'code': {str(p): digest(p) for p in tracked},
            'labels': 'Mock | 2.0.8 | app; Portuguese app; difference image uses exact mock pixels',
            'sheet_rows': ['entry', 'focus', 'exit', 'reverse'], 'times_ms': TIMES}
manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
print(f'{len(artifacts)} proofs assembled; source BMPs retained without conversion')
