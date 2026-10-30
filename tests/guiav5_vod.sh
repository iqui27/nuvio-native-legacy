#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/vodbotoes
mkdir -p "$out"
# Reuse unchanged objects across visual iterations; header changes invalidate all.
work="$out/.build"
mkdir -p "$work"

export NUVIO_DADOS="$work/data"
mkdir -p "$NUVIO_DADOS"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guiav5ui.c) continue;; esac
  sources+=("$source")
done
# Build shared objects once for before/after proof; UI is included by the test.
flags=(-Isrc -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations)
objects=()
for source in "${sources[@]}"; do
  object="$work/$(basename "${source%.c}").o"
  if [[ ! -f "$object" || "$source" -nt "$object" ]] || [[ -n $(find src -name '*.h' -newer "$object" -print -quit 2>/dev/null) ]]; then
    cc "${flags[@]}" -c "$source" -o "$object"
  fi
  objects+=("$object")
done
libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
git show 1e7b771d6771d95e47db73cd237c0f40fe60f751:src/guiav5ui.c > "$work/before.c"
cc "${flags[@]}" -DVOD_BASELINE -DGUIAV5_UNDER_TEST=\""$work/before.c"\" tests/guiav5_vod.c "${objects[@]}" "${libs[@]}" -o "$work/before"
"$work/before" "$out"
cc "${flags[@]}" tests/guiav5_vod.c "${objects[@]}" "${libs[@]}" -o "$work/after"
"$work/after" "$out"
magick identify -format '%f: %wx%h\n' "$out/before.png" "$out/after.png"
