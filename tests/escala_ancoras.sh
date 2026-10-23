#!/bin/bash
# Quick Home 85/100/130% + Standard episode proof, including committed failures.
# NV_ANCHOR_FULL=1 retains the full Home/Search/Detail/Settings/clock matrix.
set -euo pipefail
SECONDS=0
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp/jobtmp}"
mkdir -p "$TMPDIR"
w=$(mktemp -d "$TMPDIR/nv-ancoras.XXXXXX")
trap 'rm -rf "$w"' EXIT
export NV_ANCHOR_SHOTS="${NV_ANCHOR_SHOTS:-/Volumes/ExternalSSD/tmp/rev/escalafixa-shots}"
mkdir -p "$NV_ANCHOR_SHOTS"
# Optional independent failure proof against a committed production revision.
if [ -n "${NV_ANCHOR_BASELINE:-}" ]; then
  mkdir -p "$w/base" "$w/fixtures"
  for source in home.c busca.c detail.c ajustes.c ajustes_ux_desenho.inc ajustes_ux_v2.inc; do
    git show "$NV_ANCHOR_BASELINE:src/$source" > "$w/base/$source"
  done
  python3 - "$w" <<'PY'
import pathlib, sys
work=pathlib.Path(sys.argv[1])
for source in pathlib.Path('tests').glob('escala_ancoras*.c'):
    text=source.read_text().replace('../src/',str(work/'base')+'/')
    (work/'fixtures'/source.name).write_text(text)
PY
fi
flags=(-DAJUSTES_TESTE -O0 -w -Isrc -Itests -I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
# Instrument the renderer itself, including internal gfx_cor/material calls.
python3 - "$w/gfx.c" <<'PY'
import pathlib, sys
source=pathlib.Path('src/gfx.c').read_text()
needle='  GFX_TR_RECT(r.x, r.y, r.w, r.h);'
assert needle in source
source=source.replace(needle, '  anchor_probe(r, tex, modo, foco, parx, pary, raio, cr, cg, cb, ca);\n'+needle,1)
pathlib.Path(sys.argv[1]).write_text('#include "gfx.h"\nvoid anchor_probe(GfxRect, GLuint, GfxModo, float, float, float, float, float, float, float, float);\n'+source)
PY
srcs=("$w/gfx.c")
for s in src/*.c src/dts/*.c tests/escala_ancoras*.c; do
  case "$s" in src/main.c|src/gfx.c|src/home.c|src/busca.c|src/detail.c|src/ajustes.c) continue;; esac
  srcs+=("$s")
done
if [ -n "${NV_ANCHOR_BASELINE:-}" ]; then
  filtered=()
  for source in "${srcs[@]}"; do
    case "$source" in tests/escala_ancoras*.c) filtered+=("$w/fixtures/${source##*/}");; *) filtered+=("$source");; esac
  done
  srcs=("${filtered[@]}")
fi
printf '%s\n' "${srcs[@]}" | xargs -P 12 -I{} sh -c \
  'cc "$@" -c "$0" -o "'"$w"'/$(echo "$0" | tr / _).o"' {} "${flags[@]}"
cc "$w"/*.o -Wl,-dead_strip -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -o "$w/t"
NUVIO_DADOS="$w" "$w/t"
if [ -z "${NV_ANCHOR_FULL:-}${NV_ANCHOR_BASELINE:-}${NV_ANCHOR_UPCOMING:-}" ]; then
  # Reuse support objects; replace only the two production modules under review.
  git show f420d6d0e:src/home.c > "$w/home.c"
  git show d3fff84c9:src/detail.c > "$w/detail.c"
  git show d3fff84c9:src/detail.h > "$w/detail.h"
  python3 - "$w" <<'PY'
import pathlib, sys
work = pathlib.Path(sys.argv[1])
for module in ('home', 'detail'):
    fixture = pathlib.Path(f'tests/escala_ancoras_{module}.c').read_text()
    (work/f'{module}_fixture.c').write_text(fixture.replace(f'../src/{module}.c', str(work/f'{module}.c')))
PY
  for module in home detail; do
    cc "${flags[@]}" -c "$w/${module}_fixture.c" -o "$w/tests_escala_ancoras_${module}.c.o"
  done
  cc "$w"/*.o -Wl,-dead_strip -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -o "$w/baseline"
  mkdir -p "$w/baseline-data" "$NV_ANCHOR_SHOTS/baseline"
  baseline_rc=0
  NUVIO_DADOS="$w/baseline-data" NV_ANCHOR_SHOTS="$NV_ANCHOR_SHOTS/baseline" "$w/baseline" > "$w/baseline.log" 2>&1 || baseline_rc=$?
  [ "$baseline_rc" = 1 ]
  grep -E '^FAIL home-' "$w/baseline.log"
  grep -E '^FAIL detail-standard-reservation .* combined layer reserves rendered thumbnail$' "$w/baseline.log"
  printf 'PASS independent committed baselines fail (rc=%s)\n' "$baseline_rc"
fi
if [ -z "${NV_ANCHOR_FULL:-}${NV_ANCHOR_BASELINE:-}${NV_ANCHOR_UPCOMING:-}" ]; then
  [ "$SECONDS" -lt 50 ]
  printf 'PASS quick anchor budget: %ss < 50s\n' "$SECONDS"
fi
