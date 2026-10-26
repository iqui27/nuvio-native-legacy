#!/bin/bash
# Local deterministic review comparison and soundtrack transition evidence.
set -euo pipefail
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/j6-reviews
export OBJ=/tmp/nv21-obj-$(basename "$PWD")
export NUVIO_SHOT_TIME=1792971600 NUVIO_SHOT_SOURCE=1 NUVIO_SHOT_AMIGOS=1
export NUVIO_PROOF_ASSETS=/Volumes/ExternalSSD/tmp/rev/j6-r4/assets
mkdir -p "$out"/{reference,integration,before,after,music}
git show integracao/2.1:src/detail.c > "$out/reference/detail.c"
# Capture harness compatibility; unused by the isolated review renderer.
printf '\nstatic Uint32 heroDesde;\n' >> "$out/reference/detail.c"
git show d0d829b74:src/detail.c > "$out/reference/before-detail.c"
for stage in integration before after; do
  source=()
  if [ "$stage" = integration ]; then source=("NUVIO_DETAIL_SOURCE=$out/reference/detail.c"); fi
  if [ "$stage" = before ]; then source=("NUVIO_DETAIL_SOURCE=$out/reference/before-detail.c"); fi
  env NUVIO_DETAIL21_PROOF=1 "${source[@]}" bash tests/detalhe_glass_shot.sh "$out/$stage" reviews-proof > "$out/$stage.log" 2>&1
done
for setting in '100 1' '85 0'; do
  read -r scale glass <<< "$setting"
  NUVIO_DETAIL21_PROOF=1 NUVIO_PROOF_SECTION=music NUVIO_PROOF_SCALE="$scale" NUVIO_PROOF_GLASS="$glass" \
    bash tests/detalhe_glass_shot.sh "$out/music" legacy-proof > "$out/music-$scale.log" 2>&1
done
python3 - <<'PY'
import subprocess
from pathlib import Path
out = Path('/Volumes/ExternalSSD/tmp/rev/j6-reviews')
for reference in sorted((out / 'integration').glob('*.png')):
    current = out / 'after' / reference.name
    result = subprocess.run(['magick', 'compare', '-metric', 'AE', str(reference), str(current), 'null:'], capture_output=True, text=True)
    print(f'{reference.name}: differing pixels={result.stderr.strip()}')
    assert result.returncode == 0, reference.name
print('PASS: integration review pixels match at 100% glass and 85% solid')
PY
