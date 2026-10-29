#!/bin/bash
# Production V1 visual/regression check, including the phone/touch paths.
set -eu
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
mkdir -p "$TMPDIR"
output="${1:-/Volumes/ExternalSSD/tmp/rev/fileirav1}"
mkdir -p "$output"
export NUVIO_DADOS
NUVIO_DADOS=$(mktemp -d "$TMPDIR/nuvio-fileirarepensar-shot.XXXXXX")
trap 'rm -rf "$NUVIO_DADOS"' EXIT
export http_proxy=http://127.0.0.1:9 https_proxy=http://127.0.0.1:9
export HTTPS_PROXY=http://127.0.0.1:9 ALL_PROXY=http://127.0.0.1:9 all_proxy=http://127.0.0.1:9
unset no_proxy NO_PROXY
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/home.c) continue;; esac
  sources+=("$source")
done
# Keep objects outside the repo; visual iteration recompiles only the harness.
mkdir -p "$output/objects-touch"
objects=()
for source in "${sources[@]}"; do
  object="$output/objects-touch/${source//\//_}.o"
  objects+=("$object")
  if [ ! -f "$object" ] || [ "$source" -nt "$object" ] ||
     [ -n "$(find src -name '*.h' -newer "$object" -print -quit)" ]; then
    cc -c "$source" -Isrc -o "$object" -O1 -g -DNV_TOUCH_UI \
      -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
      -Wno-deprecated-declarations -Wno-macro-redefined >>"$output/build.log" 2>&1
  fi
done
cc "${objects[@]}" tests/fileirarepensar_shot.c -Isrc -o "$NUVIO_DADOS/shot" \
  -O1 -g -DNV_TOUCH_UI -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined >>"$output/build.log" 2>&1
"$NUVIO_DADOS/shot" "$output" >"$output/render.log" 2>&1
magick "$output/animation-0.png" "$output/animation-1.png" \
  "$output/animation-2.png" "$output/animation-3.png" +append "$output/animation-strip.png"
magick /Volumes/ExternalSSD/tmp/rev/fileirarepensar/v1.png \
  "$output/tv-first.png" +append "$output/v1-side-by-side.png"
echo "Production V1 TV/phone evidence and complete transition endpoints: $output"
