#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
OBJ=${OBJ:-/tmp/nv21-detsecoes-shot}
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-detail-touch.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined)
mkdir -p "$OBJ"
objs=()
for s in src/*.c src/dts/*.c; do
  case "$s" in src/main.c|src/detail.c) continue;; esac
  if [[ "$s" == src/layout.c || "$s" == src/ponteiro.c ]]; then
    cc "${flags[@]}" -DNV_TOUCH_UI -c "$s" -o "$work/$(basename "$s" .c).o"
    objs+=("$work/$(basename "$s" .c).o")
  else
    o="$OBJ/$(basename "$s" .c).o"
    if [ ! -f "$o" ] || [ "$s" -nt "$o" ] || [ -n "$(find src -name '*.h' -newer "$o" -print -quit)" ]; then
      cc "${flags[@]}" -c "$s" -o "$o"
    fi
    objs+=("$o")
  fi
done
cc "${flags[@]}" tests/detail_lower_touch.c "${objs[@]}" -o "$work/touch" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
NUVIO_DADOS="$work" "$work/touch"
