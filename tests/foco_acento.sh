#!/bin/bash
# Accent-colour focus plate: OFF identical to the gray plate, ON in the accent and legible.
#   bash tests/foco_acento.sh            (assertions)
#   FOCO_STAGE=/tmp/foco bash tests/foco_acento.sh   (also saves PNGs)
# Objects are cached in $TMPDIR/nuvio-foco-obj so reruns only rebuild what changed.
set -eu
cd "$(dirname "$0")/.."
obj="${TMPDIR:-/tmp}/nuvio-foco-obj"
mkdir -p "$obj"
flags=(-DNV_TOUCH_UI -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined -w)
objs=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  o="$obj/$(echo "$source" | tr / _).o"
  objs+=("$o")
  if [ ! -f "$o" ] || [ "$source" -nt "$o" ] ||
     [ -n "$(find src -type f \( -name '*.h' -o -name '*.inc' \) -newer "$o" -print -quit)" ]; then
    while [ "$(jobs -r | wc -l)" -ge 8 ]; do sleep 0.2; done
    cc "${flags[@]}" -c "$source" -o "$o" &
  fi
done
wait
binary="$(mktemp "${TMPDIR:-/tmp}/nuvio-foco-acento.XXXXXX")"
trap 'rm -f "$binary"' EXIT
cc "${flags[@]}" "${objs[@]}" tests/foco_acento.c -o "$binary" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$binary"
