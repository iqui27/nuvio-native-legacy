#!/bin/bash
# Local 1080p fixtures only; no TV, account or network operations.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guide-live-mv.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work"
out=${1:-/Volumes/ExternalSSD/tmp/rev/21-guide-live-mv-followup}
mkdir -p "$out"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/guide_live_mv_navigation.c -Isrc -o "$work/test" -O0 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
# Separate processes keep the legacy account fixture's static state isolated.
"$work/test" --accounts
"$work/test" "$out"
