#!/bin/bash
# Offline gate for FPSBASE lines captured on the SAME device/settings.
# bash tests/fps_gate.sh --compara baseline.txt run.log [tolerance-percent]
# bash tests/fps_gate.sh --grava run.log baseline.txt
# With no arguments, exercise the runner and gate headless.
set -euo pipefail
cd "$(dirname "$0")/.."
if [ "$#" -eq 0 ]; then exec bash tests/fpscena.sh; fi
python3 - "$@" <<'PY'
import math
import pathlib
import sys


def read(path, log):
    # log: raw program output, only FPSBASE lines count, the rest is noise.
    # baseline: strict, `<scene> <p95> <upl>` (FPSBASE prefix optional).
    scenes = {}
    for line in pathlib.Path(path).read_text().splitlines():
        if not line or line.startswith('#'):
            continue
        fields = line.split()
        if fields[0] == 'FPSBASE':
            fields = fields[1:]
        elif log:
            continue
        if len(fields) != 3:
            raise ValueError(f'invalid baseline/sample: {line}')
        name, p95, uploads = fields
        p95, uploads = float(p95), int(uploads)
        if not math.isfinite(p95) or p95 < 0 or uploads < 0 or name in scenes:
            raise ValueError(f'invalid/duplicate scene: {name}')
        scenes[name] = p95, uploads
    if not scenes:
        raise ValueError('no FPSBASE samples')
    return scenes


try:
    mode = sys.argv[1]
    if mode == '--grava' and len(sys.argv) == 4:
        samples = read(sys.argv[2], True)
        pathlib.Path(sys.argv[3]).write_text(''.join(
            f'{name} {p95:.3f} {uploads}\n' for name, (p95, uploads) in samples.items()))
        sys.exit(0)
    if mode != '--compara' or len(sys.argv) not in (4, 5):
        raise ValueError('use --compara baseline run [percent] or --grava run baseline')
    baseline, samples = read(sys.argv[2], False), read(sys.argv[3], True)
    pct = float(sys.argv[4]) if len(sys.argv) == 5 else 10.0
    if not math.isfinite(pct) or pct < 0:
        raise ValueError('invalid tolerance')
    if baseline.keys() != samples.keys():
        raise ValueError('scene coverage differs from baseline')
    failures = 0
    for name, (before, uploads) in baseline.items():
        after, current_uploads = samples[name]
        limit = before * (1 + pct / 100) + 0.5
        passed = after <= limit and current_uploads <= uploads * 1.25 + 2
        failures += not passed
        print(f'[fps-gate] {name} {"OK" if passed else "FALHA"} '
              f'p95 before={before:.3f} after={after:.3f} limit={limit:.3f} ms '
              f'uploads before={uploads} after={current_uploads}')
    sys.exit(1 if failures else 0)
except (OSError, ValueError) as error:
    print(f'[fps-gate] FALHA {error}', file=sys.stderr)
    sys.exit(1)
PY
