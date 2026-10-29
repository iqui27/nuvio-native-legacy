#!/bin/bash
# Offline Home GPU budget, idle and moving between rows (Moderna + Frost,
# Android gfx tier). NUVIO_HOMELERDO_BASELINE=<rev> runs the old src/gfx.c.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-homelerdo.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-O1 -DNV_SHOT_HOOKS -DAJUSTES_TESTE -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wno-deprecated-declarations -Wno-macro-redefined -include tests/homelerdo_gl.h)
libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
gfx_source=src/gfx.c
if [ -n "${NUVIO_HOMELERDO_BASELINE:-}" ]; then
  git show "$NUVIO_HOMELERDO_BASELINE:src/gfx.c" > "$work/gfx.c"; gfx_source="$work/gfx.c"
fi
ls src/*.c src/dts/*.c tests/homelerdo.c | grep -vE '^src/(main|gfx)\.c$' |
  xargs -P 10 -I{} sh -c 'cc "$@" -c {} -o "'"$work"'/$(echo {} | tr / _).o"' _ "${flags[@]}"
cc "${flags[@]}" -DNV_ANDROID -Isrc -c "$gfx_source" -o "$work/gfx.o"
cc "$work"/*.o "${libs[@]}" -o "$work/test"
mkdir -p "$work/data"
NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" \
  sandbox-exec -p '(version 1) (allow default) (deny network*)' "$work/test" "${1:-}"
