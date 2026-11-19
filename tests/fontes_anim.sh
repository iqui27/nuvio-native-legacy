#!/bin/bash
# Offline native GL regression and 25/50/75/100% opening strip.
set -euo pipefail
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/fontesanim
work=$(mktemp -d "${TMPDIR:-/tmp}/nv-fontes-anim.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$out" "$work/data" "$work/obj"
flags=(-DNV_SHOT_HOOKS -Isrc -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined)
export NV_FONTES_OBJECTS="$work/obj"
sources=()
for source in src/*.c src/dts/*.c; do
  [[ "$source" != src/main.c && "$source" != src/streams.c ]] && sources+=("$source")
done
printf '%s\n' "${sources[@]}" tests/fontes_anim.c |
  xargs -P 8 -I{} sh -c 'cc "$@" -c "$0" -o "$NV_FONTES_OBJECTS/$(basename "$0").o"' '{}' "${flags[@]}"
cc "$work/obj/"*.o -o "$work/test" -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
for mode in warm cold full reduced solid; do
  NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" "$work/test" "$out" "$mode"
done
