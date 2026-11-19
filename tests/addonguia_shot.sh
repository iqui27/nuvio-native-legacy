#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
export NUVIO_DADOS="$(mktemp -d /tmp/nuvio-addonguia-shot.XXXXXX)"
trap 'rm -rf "$NUVIO_DADOS"' EXIT
out="${1:-/Volumes/ExternalSSD/tmp/rev/addon-guia-png}"
mkdir -p "$out"
if [ "${AG_SHOT_BIN:-}" ]; then
  "$AG_SHOT_BIN" "$out"
  exit
fi
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in */main.c) continue;; esac
  sources+=("$source")
done
cc -DNV_SHOT_HOOKS -DAJUSTES_TESTE "${sources[@]}" tests/addonguia_shot.c -Isrc -o /tmp/nuvio-addonguia-shot \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations
/tmp/nuvio-addonguia-shot "$out"
if [ -z "${AG_SHOT_PASSES:-}" ]; then python3 tests/addonguia_png.py "$out"; fi
