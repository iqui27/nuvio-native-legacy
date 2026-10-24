#!/bin/bash
# Native offline regression and English/German PNG proof; no user data touched.
set -eu
cd "$(dirname "$0")/.."
output=${1:-/Volumes/ExternalSSD/tmp/rev/dicacreditos-png}
phase=${2:-after}
mkdir -p "$output"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-dicacreditos.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-DNV_SHOT_HOOKS)
if [ "$phase" = before ]; then
  git show c92539ccad18b81a95994c24b3591860e8567fed:src/posplay.c > "$work/posplay.c"
  flags+=("-DNV_POSPLAY_SOURCE=\"$work/posplay.c\"")
fi
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ] && [ "$source" != src/posplay.c ]; then sources+=("$source"); fi
done
cc "${flags[@]}" "${sources[@]}" tests/dicacreditos.c -Isrc -o "$work/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$work/test" "$output" "$phase"
