#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
OBJ=/tmp/nv21-resto-shot-obj
mkdir -p "$OBJ"
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined)
objs=()
for s in src/*.c src/dts/*.c; do
  case "$s" in src/main.c|src/detail.c) continue;; esac
  o="$OBJ/$(basename "$s" .c).o"
  objs+=("$o")
  if [ ! -f "$o" ] || [ "$s" -nt "$o" ] || [ -n "$(find src -name '*.h' -newer "$o" -print -quit)" ]; then
    cc "${flags[@]}" -c "$s" -o "$o"
  fi
done
cc "${flags[@]}" tests/detail21.c "${objs[@]}" -o "$OBJ/detail21" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
D=$(mktemp -d /tmp/nv21-resto-test.XXXXXX)
trap 'rm -rf "$D"' EXIT
NUVIO_DADOS="$D" "$OBJ/detail21" "$@"
