#!/bin/bash
# PR #619 regressions and guide v5 integration, with offline native PNG proof.
set -euo pipefail
cd "$(dirname "$0")/.."
for suite in esportes esportes_estado esportes_ordem esportes_ui fontescanais_folha streamfit_sheet guiav5 guiav5_epg_lifetime; do
  bash "tests/$suite.sh"
done
python3 tests/esportes_arte.py
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-esportes619.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/dados"
mkdir -p "$NUVIO_DADOS" /Volumes/ExternalSSD/tmp/rev/esportes619
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c|src/esportes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/esportes_v5_shot.c -Isrc -o "$work/shot" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations
"$work/shot" /Volumes/ExternalSSD/tmp/rev/esportes619
