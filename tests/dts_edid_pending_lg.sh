#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-edid-pending.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
export NUVIO_DADOS="$tmp/dados"
mkdir -p "$NUVIO_DADOS"
flags=(-std=c11 -D_GNU_SOURCE -DNV_DTS_EDID_PENDING_TEST -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/video.c) continue;; esac
  sources+=("$source")
done
cc "${flags[@]}" "${sources[@]}" tests/dts_edid_pending_lg.c -o "$tmp/test" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -pthread -ldl
"$tmp/test"
