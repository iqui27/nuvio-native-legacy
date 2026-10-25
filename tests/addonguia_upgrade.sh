#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
out="${1:-/Volumes/ExternalSSD/tmp/rev/21-21-c4}"
mkdir -p "$out"
export NUVIO_DADOS="$(mktemp -d /tmp/nuvio-home21-shot.XXXXXX)"
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
cc -DNV_SHOT_HOOKS "${sources[@]}" tests/addonguia_upgrade.c -Isrc -o "$NUVIO_DADOS/test" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations >"$out/build-test.log" 2>&1 || { tail -40 "$out/build-test.log"; exit 1; }
"$NUVIO_DADOS/test" "$out/home-upgrade" >"$out/test.log" 2>&1 || { tail -30 "$out/test.log"; exit 1; }
for f in "$out"/home-upgrade-*.bmp; do
  [ -e "$f" ] || continue
  sips -s format png "$f" --out "${f%.bmp}.png" >/dev/null
  rm "$f"
done
tail -15 "$out/test.log"
