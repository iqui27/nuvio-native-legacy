#!/bin/bash
# One focused offline check, with phone proof images.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-m3.XXXXXX")
trap 'if [ "${NUVIO_M3_KEEP:-0}" != 1 ]; then rm -rf "$work"; fi' EXIT
mkdir -p /Volumes/ExternalSSD/tmp/rev/ios-m3
printf '%s\n' "$work" > /Volumes/ExternalSSD/tmp/rev/ios-m3/test-work.txt
export NUVIO_DADOS="$work/data"
mkdir -p "$NUVIO_DADOS"
out="${NUVIO_M3_PROOF:-/Volumes/ExternalSSD/tmp/rev/ios-m3/raw}"
mkdir -p "$out"
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
objects=()
cache=/Volumes/ExternalSSD/tmp/rev/ios-m3/phone-objects
mkdir -p "$cache"
for source in "${sources[@]}" tests/ios_m3.c; do
  object="$cache/$(basename "$source" .c).o"
  if [ ! -f "$object" ] || [ "$source" -nt "$object" ] || [ -n "$(find src \( -name '*.inc' -o -name '*.h' \) -newer "$object" -print -quit)" ]; then
    ccache cc -DNV_TOUCH_UI -DNV_SHOT_HOOKS -Isrc -c "$source" -o "$object" \
      -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
      -Wno-deprecated-declarations -Wno-macro-redefined
  fi
  objects+=("$object")
done
cc "${objects[@]}" -o "$work/check" -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$work/check" "$out"
