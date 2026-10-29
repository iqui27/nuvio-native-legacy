#!/bin/bash
# Offscreen PNGs of the Apple TV sidebar (see menu_tvos_shot.c).
set -eu
cd "$(dirname "$0")/.."
saida="${1:-${TMPDIR:-/tmp}/nv-menutvos}"
mkdir -p "$saida"
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tvos-shot.XXXXXX"); export NUVIO_DADOS
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tvos-build.XXXXXX")
trap 'rm -rf "$NUVIO_DADOS" "$work"' EXIT
flags=(-O1 -g -DNV_SHOT_HOOKS -DNV_TOUCH_UI -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wno-deprecated-declarations -Wno-macro-redefined)
libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
objects=()
for source in src/*.c src/dts/*.c tests/menu_tvos_shot.c; do
  case "$source" in src/main.c|src/menu.c) continue;; esac
  object="$work/$(basename "$source" .c).o"
  cc "${flags[@]}" -c "$source" -o "$object"
  objects+=("$object")
done
cc "${objects[@]}" "${libs[@]}" -o "$work/shot"
"$work/shot" "$saida"
