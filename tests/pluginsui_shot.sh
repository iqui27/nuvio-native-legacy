#!/bin/bash
# Capturas da tela de Plugins (tests/pluginsui_shot.c). Sem rede.
#   PLUG_MANIFESTO=<manifest.json> bash tests/pluginsui_shot.sh [/tmp/prefixo]
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/pluginsui-shot.XXXXXX")
NUVIO_DADOS="$tmp/dados"; mkdir -p "$NUVIO_DADOS"; export NUVIO_DADOS
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do case "$source" in src/main.c) continue;; esac; sources+=("$source"); done
cc "${sources[@]}" tests/pluginsui_shot.c -Isrc -o "$tmp/shot" -O0 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 -lSDL2_image \
  -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
"$tmp/shot" "$@"
