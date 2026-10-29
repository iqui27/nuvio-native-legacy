#!/bin/bash
# Local GL regression and before/after PNGs; no deployment or network.
set -euo pipefail
cd "$(dirname "$0")/.."
OBJ=${OBJ:-/tmp/nv21-detsecoes-shot}
out=/Volumes/ExternalSSD/tmp/rev/detinfo/${1:-after}
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-detinfo.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$OBJ" "$out"
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined)
objs=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/detail.c) continue;; esac
  object="$OBJ/$(basename "$source" .c).o"
  case "$source" in
    src/layout.c|src/gfx.c|src/ponteiro.c)
      object="$work/$(basename "$source" .c).o"
      cc "${flags[@]}" -DNV_TOUCH_UI -c "$source" -o "$object";;
    *)
      if [ ! -f "$object" ] || [ "$source" -nt "$object" ] || [ -n "$(find src -name '*.h' -newer "$object" -print -quit)" ]; then
        cc "${flags[@]}" -c "$source" -o "$object"
      fi;;
  esac
  objs+=("$object")
done
extra=()
if [ -n "${NUVIO_DETAIL_SOURCE:-}" ]; then extra+=("-DNV_DETAIL_SOURCE=\"$NUVIO_DETAIL_SOURCE\""); fi
cc "${flags[@]}" ${extra[@]+"${extra[@]}"} tests/detail_detinfo.c "${objs[@]}" -o "$work/proof" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
NUVIO_DADOS="$work" "$work/proof" "$out"
if [ "${1:-after}" = after ]; then
  OBJ="$OBJ" bash tests/detail_lower_sections.sh
fi
