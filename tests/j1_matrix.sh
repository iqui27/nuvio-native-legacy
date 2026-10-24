#!/bin/bash
# Reuse the J1 capture executables for scale, rail, layout and accent coverage.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - "$1" <<'PY'
import json, os, subprocess, sys, tempfile
from pathlib import Path
out = Path(sys.argv[1]).resolve()
results = []
variants = [(s, 'moderna', 0, 0) for s in ('.9', '1.1', '1.2', '1.3')]
variants += [('.85', rail, layout, accent)
             for rail, layout in [('fixa', 0), ('recolhida', 0), ('moderna', 1), ('dinamica', 2)]
             for accent in (0, 6)]
for screen in ('library', 'glance', 'rewatch'):
    executable = out / screen / 'capture'
    if not executable.exists():
        raise SystemExit(f'Missing capture executable: {executable}')
    for scale, rail, layout, accent in variants:
        dest = out / screen / 'matrix' / f'{scale}-{rail}-L{layout}-A{accent}'
        dest.mkdir(parents=True, exist_ok=True)
        env = os.environ.copy()
        env.update(NV_J1_SHOT='1', NV_J1_STILLS='1', NUVIO_SHOT_ESCALA_TEXTO=scale,
                   NUVIO_SHOT_VIDRO='1', NUVIO_RAIL=rail, NUVIO_SHOT_PT='1',
                   NUVIO_SHOT_ACCENT=str(accent), NUVIO_SHOT_LAYOUT=str(layout), NV_REWATCH_SHOT='1')
        with tempfile.TemporaryDirectory(prefix='nuvio-j1-matrix-') as data:
            env['NUVIO_DADOS'] = data
            command = [str(executable), str(dest / ('native' if screen == 'library' else screen))]
            if screen == 'library':
                command.append('mock')
            with (dest / 'capture.log').open('w') as log:
                rc = subprocess.call(command, env=env, stdout=log, stderr=subprocess.STDOUT)
        results.append(dict(screen=screen, scale=scale, rail=rail, layout=layout, accent=accent, rc=rc))
        (out / 'matrix.json').write_text(json.dumps(results, indent=2))
        if rc:
            raise SystemExit(rc)
print(f'J1 matrix: {len(results)} capture runs passed')
PY
