#!/bin/bash
# Safe-area math plus native rendering and pointer bounds.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tela21.XXXXXX")
trap 'rm -rf "$work"' EXIT
if [ "$(uname -s)" = Darwin ]; then
  strip=(-Wl,-dead_strip)
else
  strip=(-Wl,--gc-sections)
fi
# Probe the actual migrated origins, not a duplicate formula.
for screen in biblioteca comecar conquistas filmo; do
  case "$screen" in
    biblioteca) origin='return paginaX();';;
    comecar) origin='origem(); return ox + MARGEM;';;
    conquistas) origin='origem(); return ox + MG;';;
    filmo) origin='return paginaX() + 72;';;
  esac
  printf '#include "%s/src/%s.c"\nfloat t21_probe_%s(void) { %s }\n' \
    "$PWD" "$screen" "$screen" "$origin" > "$work/$screen.c"
done
cc tests/tela21.c "$work"/*.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -O1 -ffunction-sections -fdata-sections "${strip[@]}" -lm -o "$work/test"
"$work/test"
# Instrument only the two production screens; keep the real renderer and input.
for screen in filmo conquistas; do
  printf '#include "%s/tests/tela21_shot.h"\n#include "%s/src/%s.c"\n' \
    "$PWD" "$PWD" "$screen" > "$work/render-$screen.c"
done
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/filmo.c|src/conquistas.c) continue;; esac
  sources+=("$source")
done
cc -DAJUSTES_TESTE "${sources[@]}" "$work"/render-*.c tests/tela21_shot.c -Isrc \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -o "$work/render"
export NUVIO_DADOS="$work/data"
mkdir -p "$NUVIO_DADOS"
out="${1:-/Volumes/ExternalSSD/tmp/rev/diretor21/j0/after}"
mkdir -p "$out"
"$work/render" "$out"
