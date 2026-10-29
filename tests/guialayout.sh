#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guialayout.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/dados"
mkdir -p "$NUVIO_DADOS"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c) continue;; esac
  sources+=("$source")
done
cache="${TMPDIR:-/tmp}/nuvio-guialayout-$(basename "$PWD")"
mkdir -p "$cache"
flags=(-Isrc -O0 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations)
objects=()
for source in "${sources[@]}"; do
  object="$cache/$(basename "$source").o"
  if [[ ! -f "$object" || "$source" -nt "$object" ]]; then
    cc "${flags[@]}" -c "$source" -o "$object"
  fi
  objects+=("$object")
done
cc "${flags[@]}" tests/guialayout.c "${objects[@]}" -o "$work/test" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$work/test"
video_objects=()
for object in "${objects[@]}"; do
  [[ "$object" == */player.c.o ]] || video_objects+=("$object")
done
cc "${flags[@]}" tests/guialayout_video.c src/guia.c src/guiav5ui.c "${video_objects[@]}" -o "$work/video" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$work/video"
bash tests/guiav5.sh
bash tests/guiav5_epg_lifetime.sh
