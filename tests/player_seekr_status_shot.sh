#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -euo pipefail
cd "$(dirname "$0")/.."
saida="${1:-${TMPDIR:-/tmp}/nuvio-player-seekr-status}"
mkdir -p "$saida"
taskDados=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-player-seekr-status-shot.XXXXXX)
trap 'rm -rf "$taskDados"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/player.c) continue;; esac
  sources+=("$source")
done
cc -DNV_SHOT_HOOKS "${sources[@]}" tests/player_seekr_status_shot.c -Isrc \
  -o "$taskDados/shot" -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
for vidro in 0 1; do
  NUVIO_SHOT_VIDRO=$vidro NUVIO_DADOS="$taskDados" NUVIO_TESTE_DIR="$taskDados" \
    "$taskDados/shot" "$saida"
done
for source in "$saida"/*.bmp; do
  sips -s format png "$source" --out "${source%.bmp}.png" >/dev/null
done
