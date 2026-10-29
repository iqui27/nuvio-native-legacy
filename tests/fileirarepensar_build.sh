#!/bin/bash
# Production build: the exploration macro must remain absent.
set -eu
cd "$(dirname "$0")/.."
export TMPDIR
TMPDIR=$(mktemp -d /Volumes/ExternalSSD/tmp/jobtmp/nuvio-fileirarepensar-build.XXXXXX)
trap 'rm -rf "$TMPDIR"' EXIT
# Reuse the native objects produced by the target shot check. Compile the two
# excluded translation units without NV_FICHA_EXPLORATION, then link the app.
objects=/Volumes/ExternalSSD/tmp/rev/fileirarepensar/objects
if [ ! -d "$objects" ]; then
  bash tools/build-local.sh
  exit 0
fi
for source in main home; do
  cc -c "src/$source.c" -Isrc -o "$TMPDIR/$source.o" -O1 -g \
    -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations
done
cc "$objects"/*.o "$TMPDIR/main.o" "$TMPDIR/home.o" -o "$TMPDIR/app" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
if nm "$TMPDIR/app" | grep -q 'fichaExploration\|desenhaFichaExploration'; then
  echo 'Exploration leaked into the production build' >&2; exit 1
fi
echo 'Production app linked; exploration switch absent.'
