#!/bin/bash
# Capturas do player no Glass UI, nos dois materiais. Nao entra na suite
# (*_shot.sh): janela GL.
#   NUVIO_SHOT_MOCK=<pasta do player-mockup> bash tests/player_glass_shot.sh <dir> [quadro...]
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-player-glass.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" = src/main.c ] && continue
  if [ "$source" = src/player.c ] && [ -n "${ANTES_PLAYER:-}" ]; then
    sources+=("$ANTES_PLAYER")
  else
    sources+=("$source")
  fi
done
cc -DNV_SHOT_HOOKS "${sources[@]}" tests/player_glass_shot.c -Isrc -o "$work/shot" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
mkdir -p "$1" "$work/dados"
for v in 0 1; do
  NUVIO_SHOT_VIDRO=$v NUVIO_DADOS="$work/dados" NUVIO_TESTE_DIR="$work/dados" "$work/shot" "$@"
done
