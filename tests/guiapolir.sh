#!/bin/bash
# Native screenshots plus real SDL navigation; baseline remains available from the parent revision.
set -euo pipefail
cd "$(dirname "$0")/.."
out=${1:-/Volumes/ExternalSSD/tmp/rev/guiapolir/after}
mkdir -p "$out"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guiapolir.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/data" NUVIO_SHOT_POLIR=1
mkdir -p "$NUVIO_DADOS"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c) continue;; esac
  sources+=("$source")
done
extra=()
if [[ ${GUIAPOLIR_BASELINE:-0} == 1 ]]; then
  git show "${GUIAPOLIR_REF:-HEAD}:src/guiav5ui.c" > "$work/guiav5ui.c"
  extra=(-DGUIAV5_UNDER_TEST=\"$work/guiav5ui.c\" -DGUIAPOLIR_BASELINE_CODE)
fi
cc "${sources[@]}" tests/guiav5_principal_shot.c ${extra[@]+"${extra[@]}"} -Isrc -o "$work/test" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations
"$work/test" "$out"
if [[ ${GUIAPOLIR_BASELINE:-0} != 1 ]]; then
  python3 tests/guiapolir_compare.py "$(dirname "$out")"
fi
