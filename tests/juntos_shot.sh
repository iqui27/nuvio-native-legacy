#!/bin/bash
# Capturas do Watch Together (juntosui). Nao entra na suite.
#   bash tests/juntos_shot.sh [prefixo]
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-juntos-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/juntos.c) continue;; esac
  sources+=("$source")
done
objects="${TMPDIR:-/tmp}/nuvio-juntos-shot-objects"
mkdir -p "$objects"
headers=$(find src -name '*.h' -newer "$objects" -print -quit)
for source in "${sources[@]}" tests/juntos_shot.c; do
  object="$objects/${source//\//_}.o"
  if [ ! -e "$object" ] || [ "$source" -nt "$object" ] || [ -n "$headers" ]; then
    cc -c "$source" -Isrc -o "$object" -O1 -g -DNV_LEVE -DNV_SHOT_HOOKS \
      -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -w
  fi
  objectsToLink+=("$object")
done
cc "${objectsToLink[@]}" -o "$NUVIO_DADOS/shot" -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$NUVIO_DADOS/shot" "${1:-${TMPDIR:-/tmp}/nuvio-juntos}" "${2:-}"
