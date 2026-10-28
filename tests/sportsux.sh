#!/bin/bash
# Offline Sports navigation, profile persistence, and native draw-cost proof.
set -euo pipefail
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/sportsux
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-sportsux.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$out" "$work/dados"
export NUVIO_DADOS="$work/dados" SPORTSUX_PROOF="${1:-after}"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c|src/esportes.c) continue;; esac
  sources+=("$source")
done
# Keep dependency-tracked objects outside the worktree for focused reruns.
objects="$out/objects"
make -s -j8 -f tools/native-objects.mk SOURCES="${sources[*]} tests/esportes_v5_shot.c" \
  OBJDIR="$objects" CC=cc \
  NATIVE_FLAGS='-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations'
files=()
for source in "${sources[@]}" tests/esportes_v5_shot.c; do files+=("$objects/${source%.c}.o"); done
cc "${files[@]}" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -o "$work/shot"
"$work/shot" "$out" 2>&1 | tee "$out/${SPORTSUX_PROOF}.log"
if [ "$SPORTSUX_PROOF" = after ]; then
  mkdir -p "$out/legacy" "$work/legacy-data"
  env -u SPORTSUX_PROOF NUVIO_DADOS="$work/legacy-data" "$work/shot" "$out/legacy" > "$out/legacy.log" 2>&1
  tail -1 "$out/legacy.log"
fi
