#!/bin/bash
# Offline event regression, frame-cost measurement and PNG proof.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-espevento.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/dados"
out=/Volumes/ExternalSSD/tmp/rev/espevento
mkdir -p "$NUVIO_DADOS" "$out"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c|src/esportes.c) continue;; esac
  sources+=("$source")
done
# Reproduce the base render path using the same fixture and host renderer.
if [ "${ESPEVENTO_PROOF:-after}" = before ]; then
  git show 3485af0f0:src/guiav5ui.c > "$work/guiav5ui.c"
  git show 3485af0f0:src/guiav5_esportes.inc > "$work/guiav5_esportes.inc"
  git show 3485af0f0:src/esportesui.c > "$work/esportesui.c"
  baseline=()
  for source in "${sources[@]}"; do
    if [ "$source" = src/esportesui.c ]; then baseline+=("$work/esportesui.c"); else baseline+=("$source"); fi
  done
  sources=("${baseline[@]}")
  extra=(-DGUIAV5_UNDER_TEST=\""$work/guiav5ui.c"\")
else
  extra=()
fi
cc "${sources[@]}" tests/esportes_v5_shot.c ${extra[@]+"${extra[@]}"} -Isrc -o "$work/shot" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations
ESPEVENTO_PROOF="${ESPEVENTO_PROOF:-after}" "$work/shot" "$out" 2>&1 | tee "$out/${ESPEVENTO_PROOF:-after}-perf.log"
if [ "${ESPEVENTO_PROOF:-after}" != before ]; then
  for suite in esportes esportes_estado esportes_ordem esportes_ui guiav5 guiav5_epg_lifetime lembrete; do
    bash "tests/$suite.sh"
  done
  python3 tests/esportes_arte.py
fi
