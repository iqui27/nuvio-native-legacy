#!/bin/bash
# Android pause regression, room clock, and identical headless layer workloads.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/juntos_android_pause.py
JNI_ONLY=1 bash tests/video_android_retomada.sh
bash tests/juntos_tcl.sh
python3 tests/cacheboost_android.py
out=/Volumes/ExternalSSD/tmp/rev/juntosfluido
mkdir -p "$out"
NUVIO_JUNTOS_OUT="$out" NUVIO_JUNTOS_CUSTO=sync NUVIO_JUNTOS_COMPARE=1 bash tests/juntos_cinema.sh > "$out/after.log"
python3 - <<'PY'
from pathlib import Path
out = Path('/Volumes/ExternalSSD/tmp/rev/juntosfluido')
for version in ('before', 'after'):
    lines = [s for s in (out / f'{version}.log').read_text().splitlines() if '[juntos-custo]' in s]
    assert any('lobby frames=' in s for s in lines), version
    print(version + ': ' + '\n'.join(lines))
# Compare the same animation frame; only 8-bit FBO rounding may differ.
import subprocess
comparison = subprocess.run(['magick', 'compare', '-metric', 'RMSE', str(out / '0-lobby-reference.png'),
                             str(out / '0-lobby-cost.png'), str(out / 'lobby-difference.png')],
                            capture_output=True, text=True)
assert comparison.returncode in (0, 1), comparison.stderr
rmse = float(comparison.stderr.split('(')[1].split(')')[0])
assert rmse < .005, f'cached lobby changed: RMSE={rmse}'
print(f'cached vs original same-frame RMSE={rmse}')
PY
echo 'juntos_fluido: Android pause, room clock, headless cinema OK'
