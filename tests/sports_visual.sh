#!/bin/bash
# Local 1080p presentation proof; isolated data, no network workers or input events.
set -euo pipefail
cd "$(dirname "$0")/.."
phase=${1:-after}
case "$phase" in before|after) ;; *) exit 2 ;; esac
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-sports-visual.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/data"
out="/Volumes/ExternalSSD/tmp/rev/sports-visual-followup/$phase"
mkdir -p "$NUVIO_DADOS" "$out"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c|src/esportes.c) continue;; esac
  sources+=("$source")
done
sources+=(tests/sports_visual.c)
# Reuse only compiled objects; dependencies include all production .inc files.
objects_dir=/Volumes/ExternalSSD/tmp/rev/sports-visual-followup/objects
make -s -j4 -f tools/native-objects.mk SOURCES="${sources[*]}" OBJDIR="$objects_dir" CC=cc \
  NATIVE_FLAGS='-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations'
objects=()
for source in "${sources[@]}"; do objects+=("$objects_dir/${source%.c}.o"); done
cc "${objects[@]}" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -o "$work/check"
"$work/check" "$out"
