#!/bin/bash
# #722 Spanish audio evidence characterization; local, no network, no window.
#   bash tests/fv_fon722_spanish.sh            compile + run in a temp dir
#   bash tests/fv_fon722_spanish.sh build EXE  compile only
#   bash tests/fv_fon722_spanish.sh run EXE    run an already built EXE
set -euo pipefail
cd "$(dirname "$0")/.."
build() {
  local sources=() source
  for source in src/*.c src/dts/*.c; do
    [[ "$source" != src/main.c && "$source" != src/streams.c ]] && sources+=("$source")
  done
  cc "${sources[@]}" tests/fv_fon722_spanish.c -Isrc -o "$1" \
    -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
    -Wno-deprecated-declarations -Wno-macro-redefined
}
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fv722.XXXXXX")
trap 'rm -rf "$work"' EXIT
case "${1:-}" in
  build) build "$2" ;;
  run) NUVIO_DADOS="$work" "$2" ;;
  "") build "$work/test" && NUVIO_DADOS="$work" "$work/test" ;;
  *) echo "usage: $0 [build EXE | run EXE]" >&2; exit 2 ;;
esac
