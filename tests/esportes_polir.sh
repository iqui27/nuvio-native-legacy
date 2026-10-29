#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
phase=${1:-after}
out=/Volumes/ExternalSSD/tmp/rev/espolir
mkdir -p "$out"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-espolir.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/data"
mkdir -p "$NUVIO_DADOS"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c|src/esportes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/esportes_polir_shot.c -Isrc -o "$work/shot" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations
"$work/shot" "$out" "$phase" > "$out/$phase-perf.log" 2>&1
if [ "$phase" = after ]; then
  ESPOLIR_LEGACY=1 "$work/shot" "$out" after >> "$out/$phase-perf.log" 2>&1
  FOLLOW_PNG=0 bash tests/esportes_seguir.sh
fi
python3 - "$out/$phase-perf.log" <<'PY'
import sys
for line in open(sys.argv[1]):
    if '[espolir]' in line or line.startswith('PASS'): print(line.rstrip())
PY
