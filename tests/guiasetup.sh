#!/bin/bash
# One target, shared objects, baseline FAIL->PASS and native 1080p PNGs.
set -euo pipefail
export TMPDIR=/Volumes/ExternalSSD/tmp
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/guiasetup
work="$out/build"
mkdir -p "$out/before" "$out/after" "$work"
rm -rf "$work/data-before" "$work/data-after"
mkdir -p "$work/data-before" "$work/data-after"
export NUVIO_DADOS="$work/data-before"
git show b6dafd97e:src/guiav5ui.c > "$work/baseline.c"
git show b6dafd97e:src/guia.c > "$work/guide-before.c"
git show b6dafd97e:src/guia_grupos.inc > "$work/guia_grupos.inc"
python3 tests/guiasetup_compile.py "$work"
if "$work/before" "$out/before" 1 > "$out/before/regression.log" 2>&1; then
  echo 'FAIL: baseline must reproduce source navigation/re-enable regression';exit 1
fi
rg 'FAIL: (Left on a source|re-enable immediately)' "$out/before/regression.log"
export NUVIO_DADOS="$work/data-after"
"$work/after" "$out/after" 0 > "$out/after/regression.log" 2>&1 || { tail -12 "$out/after/regression.log"; exit 1; }
tail -8 "$out/after/regression.log"
/opt/homebrew/bin/magick montage "$out"/{before,after}/setup.png "$out"/{before,after}/disabled.png "$out"/{before,after}/reenabled.png "$out"/{before,after}/vod-selected.png "$out"/{before,after}/live-footer.png -thumbnail 900x506 -tile 2x5 -geometry +8+8 -background '#121316' -font deploy/app/fonts/InterDisplay-Regular.ttf "$out/owner.jpg"
/opt/homebrew/bin/magick "$out/owner.jpg" -resize '2300x2300>' "$out/owner.jpg"
echo 'PASS: baseline FAIL->PASS; 1920x1080 before/after PNGs; owner montage below 2400px'
