#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
work=$(mktemp -d "$TMPDIR/nuvio-gifavatar.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/perfilsel.c|src/menu.c|src/gif.c) continue;; esac
  sources+=("$source")
done
flags=(-Isrc -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wno-deprecated-declarations -Wno-macro-redefined)
# Enable the same native animation implementation as Android, only in gif.c.
cc "${flags[@]}" -DNV_ANDROID -c src/gif.c -o "$work/gif.o"
cc "${flags[@]}" -DNV_PERFILSEL_TEST "${sources[@]}" tests/gifavatar.c tests/gifavatar_menu.c "$work/gif.o" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -o "$work/test"
export NUVIO_GIFAVATAR_PNG_DIR="${NUVIO_GIFAVATAR_PNG_DIR:-/Volumes/ExternalSSD/tmp/rev/gifavatar-png}"
mkdir -p "$work/data" "$NUVIO_GIFAVATAR_PNG_DIR"
set +e
NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" "$work/test" "${1:-after}"
rc=$?
set -e
for shape in wide tall; do
  sips -s format png "$NUVIO_GIFAVATAR_PNG_DIR/${1:-after}-$shape.bmp" \
    --out "$NUVIO_GIFAVATAR_PNG_DIR/${1:-after}-$shape.png" >/dev/null
  rm "$NUVIO_GIFAVATAR_PNG_DIR/${1:-after}-$shape.bmp"
done
exit "$rc"
