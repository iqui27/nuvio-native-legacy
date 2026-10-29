#!/bin/bash
# One native target: landscape FAIL/PASS, TV/portrait, existing guide checks.
set -euo pipefail
cd "$(dirname "$0")/.."
out=${1:-/Volumes/ExternalSSD/tmp/rev/guiaopcoes}
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guiaopcoes.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/data"
mkdir -p "$NUVIO_DADOS" "$out"
# Compile shared sources once; only the included UI changes for baseline proof.
git show "${GUIAOPCOES_BASELINE:-f254bbf54}:src/guiav5ui.c" > "$work/baseline.c"
python3 - "$work" <<'PY'
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess,sys
work=Path(sys.argv[1])
flags=['-O1','-g','-DNV_TOUCH_UI','-Isrc','-I/opt/homebrew/include','-I/opt/homebrew/include/SDL2','-Wno-deprecated-declarations']
sources=[p for p in [*Path('src').glob('*.c'),*Path('src/dts').glob('*.c')] if p.name not in ('main.c','guia.c','guiav5ui.c')]
def compile(p):
    obj=work/(p.stem+'.o')
    subprocess.run(['cc',*flags,'-c',str(p),'-o',str(obj)],check=True)
    return str(obj)
with ThreadPoolExecutor(max_workers=8) as pool: objects=list(pool.map(compile,sources))
for version in ('before','after'):
    extra=['-DGUIAV5_UNDER_TEST="'+str(work/'baseline.c')+'"'] if version=='before' else []
    subprocess.run(['cc',*flags,*extra,'tests/guiaopcoes.c',*objects,'-o',str(work/version),'-L/opt/homebrew/lib','-lSDL2','-lSDL2_image','-lSDL2_ttf','-lz','-framework','OpenGL'],check=True)
PY
mkdir -p "$out/before-landscape"
if "$work/before" "$out/before-landscape" 2 0 > "$out/before-landscape/fail.log" 2>&1; then
  echo 'FAIL: landscape regression must fail before the fix' >&2; exit 1
fi
rg 'FAIL: landscape header/close|FAIL: landscape D-pad|FAIL: landscape swipe' "$out/before-landscape/fail.log"
for device in tv phone landscape; do
  dir="$out/after-$device"
  mkdir -p "$dir"
  phone=0
  [[ $device == phone ]] && phone=1
  [[ $device == landscape ]] && phone=2
  "$work/after" "$dir" "$phone" 0
done
cp "$out/after-landscape/after-landscape.png" "$out/after-landscape.png"
for dir in "$out"/after-tv "$out"/after-phone; do
  /opt/homebrew/bin/magick "$dir"/opening-{0,1,2,3}.png +append "$dir/opening-strip.png"
done
echo 'PASS: landscape FAIL/PASS, TV/portrait PNGs, opening strips at 0/80/160/320 ms'
bash tests/guiav5.sh
bash tests/guiav5_bugs.sh
bash tests/guiav5_epg_lifetime.sh
