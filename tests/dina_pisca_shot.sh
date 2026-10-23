#!/bin/bash
# Dinamica ("Apple TV") + frost on the Android deferred-background path:
# ~120 frames, focus change + scroll, asserting the frost capture never reads
# the still-transparent framebuffer (the owner's flicker). Only gfx.c gets
# -DNV_ANDROID (the deferred background and the alpha-bits gate live there);
# the rest of the app stays the desktop build.
#
#   bash tests/dina_pisca_shot.sh /Volumes/ExternalSSD/tmp/pisca/dina
set -eu
cd "$(dirname "$0")/.."

saida="${1:-${TMPDIR:-/tmp}/nv-dina-pisca}"
mkdir -p "$saida"
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-dina-shot.XXXXXX")
export NUVIO_DADOS
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-dina-build.XXXXXX")
trap 'rm -rf "$NUVIO_DADOS" "$work"' EXIT

flags=(-O1 -g -DNV_SHOT_HOOKS -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wno-deprecated-declarations -Wno-macro-redefined)
libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
objects=()
for source in src/*.c src/dts/*.c tests/dina_pisca_shot.c; do
  case "$source" in src/main.c|src/gfx.c) continue;; esac
  object="$work/$(basename "$source" .c).o"
  cc "${flags[@]}" -c "$source" -o "$object"
  objects+=("$object")
done
cc "${flags[@]}" -DNV_ANDROID -c src/gfx.c -o "$work/gfx.o"
cc "${objects[@]}" "$work/gfx.o" "${libs[@]}" -o "$work/shot"
"$work/shot" "$saida"
