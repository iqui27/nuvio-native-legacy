#!/bin/bash
# Live-clock glass regression and same-run before/after PNGs, offline at 1080p.
set -eu
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
shots=/Volumes/ExternalSSD/tmp/rev/vidroc9-shots
mkdir -p "$shots"
export NUVIO_C9_CACHE=1 NUVIO_SHOT_VIDRO=1 NUVIO_SHOT_VIDRO_FOSCO=1
export NUVIO_SHOT_FUNDO=2
export NUVIO_SHOT_VIDRO_QUAL=${NUVIO_SHOT_VIDRO_QUAL:-2}
if [ $# = 0 ]; then
  if bash "$0" before > "$shots/before.log" 2>&1; then
    echo 'FAIL: baseline unexpectedly passed' >&2; exit 1
  fi
  [ "$(rg -c '^FAIL: .*frames=30' "$shots/before.log")" = 6 ]
  rg '^(FAIL|PASS): .*frames=30' "$shots/before.log"
  bash "$0" after
  NUVIO_SHOT_VIDRO_QUAL=0 bash "$0" automatic
  python3 tools/vidroc9_pixel_diff.py
  exit 0
fi
export OBJ="$TMPDIR/nuvio-vidroc9-cache-obj"
# macOS GL, with the webOS renderer branches enabled in gfx.c only.
mkdir -p "$OBJ"
gfx_source=src/gfx.c
if [ "${1:-after}" = before ]; then
  # Reproduce af7ab651f's unconditional invalidation with today's renderer API.
  python3 - "$OBJ/gfx-before.c" <<'PY'
from pathlib import Path
import sys
source = Path("src/gfx.c").read_text()
old = "if (tex) vidroEsquecerFonte(tex);"
assert source.count(old) == 1
Path(sys.argv[1]).write_text(source.replace(old, "if (tex) gfx_vidro_sujar();"))
PY
  gfx_source="$OBJ/gfx-before.c"
fi
cc -O1 -g -DNV_WEBOS -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined -c "$gfx_source" -o "$OBJ/gfx.o"
# Preserve the specialized renderer object when the shared shot runner checks headers.
touch "$OBJ/gfx.o"
label=${1:-after}
mkdir -p "$shots/$label"
bash tests/detalhe_glass_shot.sh "$shots/$label" cache
