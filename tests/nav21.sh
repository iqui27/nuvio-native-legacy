#!/bin/bash
# Review regressions plus real onboarding D-pad rules and rendered capture matrix.
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp/jobtmp}"
mkdir -p "$TMPDIR"
rc=0
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/app.c) continue;; esac
  sources+=("$source")
done
binary="$(mktemp "$TMPDIR/nav21-live.XXXXXX")"
trap 'rm -f "$binary" "$binary-idioma"' EXIT
cc tests/idioma.c src/idioma.c -Isrc -I/opt/homebrew/include \
  -I/opt/homebrew/include/SDL2 -o "$binary-idioma" \
  -Wno-macro-redefined -Wno-deprecated-declarations
"$binary-idioma" || rc=1
cc "${sources[@]}" tests/nav21_live.c -Isrc -o "$binary" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$binary" || rc=1
[ "$rc" = 0 ] || exit "$rc"
output="$TMPDIR/nav21-comecar"
mkdir -p "$output"
NUVIO_NAV21_REGRESSION=1 NUVIO_J2_MATRIX=1 NUVIO_SHOT_QUICK=1 bash tests/comecar_shot.sh "$output" || rc=1
exit "$rc"
