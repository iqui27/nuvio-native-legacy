#!/bin/bash
# One target, shared objects, baseline FAIL->PASS and native 1080p PNGs.
set -euo pipefail
export TMPDIR=/Volumes/ExternalSSD/tmp
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/guiasetup
work="$out/build"
mkdir -p "$out/before" "$out/after" "$out/blocked" "$out/owner" "$work"
rm -rf "$work/data-before" "$work/data-after" "$work/data-blocked" "$work/data-owner"
mkdir -p "$work/data-before" "$work/data-after"
export NUVIO_DADOS="$work/data-before"
git show b6dafd97e:src/guiav5ui.c > "$work/baseline.c"
git show b6dafd97e:src/guia.c > "$work/guide-before.c"
git show b6dafd97e:src/guia_grupos.inc > "$work/guia_grupos.inc"
git show 444c4cb7d:src/guiav5ui.c > "$work/blocked.c"
git show 444c4cb7d:src/guiasetup.inc > "$work/guiasetup.inc"
mkdir -p "$work/owner-src"
git show b8a20ad5f:src/guiav5ui.c > "$work/owner-src/guiav5ui.c"
git show b8a20ad5f:src/guiasetup.inc > "$work/owner-src/guiasetup.inc"
printf '#include "owner-src/guiav5ui.c"\n' > "$work/owner.c"
python3 tests/guiasetup_compile.py "$work"
mkdir -p "$work/data-blocked"
export NUVIO_DADOS="$work/data-blocked"
if "$work/blocked" "$out/blocked" 0 > "$out/blocked/regression.log" 2>&1; then
  echo 'FAIL: blocked revision must reproduce Add/cancel and rename race';exit 1
fi
rg 'FAIL: Add then Back keeps channels and EPG' "$out/blocked/regression.log"
rg 'FAIL: rename follows original source key' "$out/blocked/regression.log"
rg 'FAIL: off empty state focuses first Turn on' "$out/blocked/regression.log"
mkdir -p "$work/data-owner"
export NUVIO_DADOS="$work/data-owner"
if "$work/owner-test" "$out/owner" 0 > "$out/owner/regression.log" 2>&1; then
  echo 'FAIL: owner revision must reproduce sole-source empty setup';exit 1
fi
rg 'FAIL: off empty state focuses first Turn on' "$out/owner/regression.log"
rg 'FAIL: one-press Turn on restores cached channels' "$out/owner/regression.log"
export NUVIO_DADOS="$work/data-before"
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
