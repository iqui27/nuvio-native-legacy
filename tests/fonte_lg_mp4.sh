#!/bin/bash
# LG, auto-play + HDR "Preferir" + Dolby Vision: MP4 primeiro dentro da mesma
# faixa de resolucao. Compila streams.c duas vezes: como LG e como fora da LG
# (NV_STREAMS_LG=0). Ver tests/fonte_lg_mp4.c.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-lgmp4.XXXXXX")
trap 'rm -rf "$dir"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/streams.c) continue;; esac
  sources+=("$source")
done
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined)
libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
mkdir -p "$dir/obj"
for s in "${sources[@]}"; do
  o="$dir/obj/$(echo "$s" | tr / _).o"
  cc "${flags[@]}" -c "$s" -o "$o"
done
for lg in 1 0; do
  cc "${flags[@]}" -DNV_STREAMS_LG=$lg tests/fonte_lg_mp4.c "$dir"/obj/*.o -o "$dir/t$lg" "${libs[@]}"
  TMPDIR="$dir" "$dir/t$lg"
done
