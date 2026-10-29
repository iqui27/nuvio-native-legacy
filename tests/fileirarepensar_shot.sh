#!/bin/bash
# Compile the real native home once; render all studies without a user setting.
set -eu
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
mkdir -p "$TMPDIR"
output="${1:-/Volumes/ExternalSSD/tmp/rev/fileirarepensar}"
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
mkdir -p "$output/objects"
objects=()
for source in "${sources[@]}"; do
  object="$output/objects/${source//\//_}.o"
  objects+=("$object")
  if [ ! -f "$object" ] || [ "$source" -nt "$object" ]; then
    cc -c "$source" -Isrc -o "$object" -O1 -g \
      -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
      -Wno-deprecated-declarations -Wno-macro-redefined >>"$output/build.log" 2>&1
  fi
done
cc "${objects[@]}" tests/fileirarepensar_shot.c -Isrc -o "$NUVIO_DADOS/shot" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined >>"$output/build.log" 2>&1
"$NUVIO_DADOS/shot" "$output" >"$output/render.log" 2>&1
for variant in 1 2 3; do
  magick "$output/v$variant-frame0.png" "$output/v$variant-frame1.png" "$output/v$variant.png" \
    +append "$output/v$variant-animation.png"
done
# Exact owner reference is separate from the native baseline (no quick panel).
cp /Volumes/ExternalSSD/tmp/rev/dono-envio/tcl-foto6.png "$output/current-reference.png"
echo "Native 1080p studies and complete transition endpoints: $output"
