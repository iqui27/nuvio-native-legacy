#!/bin/bash
# #414: parser + collection navigation + actual catalog loader, no network/window.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
work="$(mktemp -d "$TMPDIR/col414.XXXXXXXX")"
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wno-deprecated-declarations -Wno-macro-redefined)
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in
    src/main.c|src/vertudo.c) continue;;
    src/descoberta.c)
      cc "${flags[@]}" -Drede_baixar=config414_baixar -Drede_baixar_st=config414_baixar_st -c "$source" -o "$work/descoberta.o"
      sources+=("$work/descoberta.o");;
    *) sources+=("$source");;
  esac
done
cc "${flags[@]}" "${sources[@]}" tests/colecao_configurator.c -o "$work/test" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
mkdir "$work/dados"
"$work/test" "$work/dados"
