#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Fixture captures of the "Recommend to a friend" sheet (tests/recenviar_shot.c).
# Usage: tests/recenviar_shot.sh <output-prefix>
#   NUVIO_SHOT_IDIOMA=6   German (any IDIOMA_* index)
#   NUVIO_SHOT_SOLIDO=1   Glass off
#   RE_FONTE=/abs/recenviar.c  photograph another version of the module
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-send-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/recenviar.c) continue;; esac
  sources+=("$source")
done
fonte=()
if [ -n "${RE_FONTE:-}" ]; then fonte=(-DRE_FONTE="\"$RE_FONTE\""); fi
cc "${sources[@]}" tests/recenviar_shot.c ${fonte[@]+"${fonte[@]}"} -Isrc -o "$NUVIO_DADOS/shot" -O1 \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$NUVIO_DADOS/shot" "$@"
