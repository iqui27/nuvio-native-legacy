#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-desmarcarfrente-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/episodios.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/desmarcarfrente.c -Isrc -o "$tmp/teste" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -w
mkdir -p "$tmp/dados"
NUVIO_DADOS="$tmp/dados" "$tmp/teste"
