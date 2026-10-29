#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/canaisjuntar
mkdir -p "$out"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-channel-shot.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/dados" NUVIO_SHOT_VARIANTS=1
mkdir -p "$NUVIO_DADOS"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/guiav5_principal_shot.c -Isrc -o "$work/shot" -O1 -g -DNV_SHOT_HOOKS \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations
"$work/shot" "$out"
