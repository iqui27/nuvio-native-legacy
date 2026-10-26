#!/usr/bin/env python3
"""Assemble J2 native captures without resizing reference components."""
import concurrent.futures
import json
from pathlib import Path
import subprocess
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else '/Volumes/ExternalSSD/tmp/rev/diretor21/21-j2/round2')
previous = root.parent
reference = Path('/Volumes/ExternalSSD/tmp/rev/linguagem/ref/reference-L0-sources.png')
server_mock = Path('/Volumes/ExternalSSD/tmp/servidores-polish')
jobs = []
outputs = []


def job(args, output):
    jobs.append(['magick', '-limit', 'thread', '1', *map(str, args), '-define', 'png:compression-level=1', str(output)])
    outputs.append(str(output))


def strip(prefix, frames):
    if all(p.exists() for p in frames):
        job(['(', *frames[:4], '-resize', '640x360', '+append', ')',
             '(', *frames[4:], '-resize', '640x360', '+append', ')', '-append'], root / (prefix + '-anim.png'))


for scale in (85, 100):
    for glass in (0, 1):
        material = 'glass' if glass else 'solid'
        prefix = f'start-{glass}-{scale}'
        for screen, mock in [('1-perfil', 1), ('2-trazer', 2), ('2b-stremio-codigo', 2),
                             ('2c-stremio-ok', 2), ('2d-aiom-link', 2), ('3-acompanhar', 3), ('4-pronto', 4)]:
            app = root / f'{prefix}-{screen}.png'
            assert app.exists(), app
            job([previous / f'mock-start-{mock}.png', reference, app, '+append'],
                root / f'start-{screen}-{scale}-{material}-tri.png')
        for step in range(1, 5):
            strip(f'start-step{step}-{scale}-{material}',
                  [root / f'{prefix}-step{step}-{kind}-{t:03}.png'
                   for kind in ('entry', 'focus') for t in (0, 80, 160, 320)])
            for kind in ('exit', 'reversal', 'page'):
                frames = [root / f'{prefix}-step{step}-{kind}-{t:03}.png' for t in (0, 80, 160, 320)]
                assert all(p.exists() for p in frames)
                job([*frames, '-resize', '640x360', '+append'], root / f'start-step{step}-{kind}-{scale}-{material}-anim.png')
        folder = root / 'servers' / f'{".85" if scale == 85 else "1"}-{glass}'
        for app in sorted(folder.glob('*-app.png')):
            name = app.name[:-8]
            mock = server_mock / (name + '.png')
            if mock.exists():
                job([mock, reference, app, '+append'], root / f'{name}-{scale}-{material}-tri.png')
                stem = 'setup' if name == 'setup-escolher' else name
                strip(f'{name}-{scale}-{material}', [folder / f'{stem}-{kind}-{t:03}-app.png'
                      for kind in ('entry', 'focus') for t in (0, 80, 160, 320)])
        bright = folder / 'setup-bright-art-app.png'
        assert bright.exists(), bright
        job([reference, bright, '+append'], root / f'setup-bright-art-{scale}-{material}-tri.png')
        for kind in ('exit', 'reversal', 'page'):
            frames = [folder / f'setup-{kind}-{t:03}-app.png' for t in (0, 80, 160, 320)]
            assert all(p.exists() for p in frames)
            job([*frames, '-resize', '640x360', '+append'], root / f'setup-{kind}-{scale}-{material}-anim.png')

# Actual-size component crops; no scaling of reference pixels.
job([reference, '-crop', '610x122+1230+210', '+repage'], root / 'reference-row-100.png')
job([root / 'start-0-100-2c-stremio-ok.png', '-crop', '860x104+104+330', '+repage'], root / 'onboarding-row-100.png')
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
    for result in pool.map(lambda command: subprocess.run(command, check=True, capture_output=True), jobs):
        assert result.returncode == 0
(root / 'pngs.json').write_text(json.dumps(outputs, indent=2) + '\n')
(root / 'index.html').write_text('<!doctype html><title>J2 round 2 proofs</title>' + ''.join(
    f'<p><a href="{Path(p).relative_to(root)}">{Path(p).name}</a></p>' for p in outputs))
print(f'PASS: {len(outputs)} PNG comparisons and animation strips')
