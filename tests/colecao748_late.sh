#!/bin/bash
# #748: colecao que abre antes do manifesto do addon (Samsung .wgt). Ver o .c.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/tmp}"
work="$(mktemp -d "$TMPDIR/col748.XXXXXXXX")"
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wno-deprecated-declarations -Wno-macro-redefined)
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in
    src/main.c|src/vertudo.c) continue;;
    src/descoberta.c)
      cc "${flags[@]}" -Drede_baixar=late748_baixar -Drede_baixar_st=late748_baixar_st -c "$source" -o "$work/descoberta.o"
      sources+=("$work/descoberta.o");;
    *) sources+=("$source");;
  esac
done
cc "${flags[@]}" "${sources[@]}" tests/colecao748_late.c -o "$work/test" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
mkdir "$work/dados"
"$work/test" "$work/dados"
