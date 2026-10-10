#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-spot-reg-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/dados"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/spotlight.c) continue;; esac
  sources+=("$source")
done
flags=()
if [ -n "${SPOTLIGHT_SOURCE:-}" ]; then
  flags+=("-DSPOTLIGHT_SOURCE=\"$SPOTLIGHT_SOURCE\"")
fi
cc "${sources[@]}" tests/spotlight_regressao.c -Isrc -o "$tmp/t" -O1 -g \
  "${flags[@]}" \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$tmp/dados" SDL_AUDIODRIVER=dummy "$tmp/t"
