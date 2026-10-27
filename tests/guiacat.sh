#!/bin/bash
# Offline guide coverage and ESPN crest pipeline, with native PNG evidence.
set -euo pipefail
cd "$(dirname "$0")/.."
# Includes 1978 + 647 + 68 category totals, mixed VOD and source power persistence.
bash tests/livecat.sh --guide-only
bash tests/xtream.sh
parser=$(mktemp "${TMPDIR:-/tmp}/nuvio-g5-kind.XXXXXX")
cc -std=gnu11 -Wall -Wextra -Werror -Isrc tests/guiav5.c src/guiav5.c src/js.c -o "$parser"
"$parser"
rm -f "$parser"
bash tests/esportes.sh
bash tests/esportes_estado.sh
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guiacat.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/dados"
mkdir -p "$NUVIO_DADOS" /Volumes/ExternalSSD/tmp/rev/guiacat
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c|src/esportes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/esportes_v5_shot.c -Isrc -o "$work/shot" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations
"$work/shot" /Volumes/ExternalSSD/tmp/rev/guiacat
