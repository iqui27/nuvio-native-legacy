#!/bin/bash
# Focused Detail regression gate. Reuse the native screenshot object's cache.
set -euo pipefail
cd "$(dirname "$0")/.."
export OBJ="${OBJ:-/tmp/nv21-detsecoes-shot}"
bash tests/detail21.sh
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined)
libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-detail-lower.XXXXXX")
trap 'rm -rf "$work"' EXIT
objs=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/detail.c|src/extras.c|src/seriefrases.c) continue;; esac
  objs+=("$OBJ/$(basename "$source" .c).o")
done
cc "${flags[@]}" -c src/detail.c -o "$work/detail.o"
cc "${flags[@]}" tests/detail_lower_feed.c "$work/detail.o" "${objs[@]}" "$OBJ/seriefrases.o" -o "$work/feed" "${libs[@]}"
NUVIO_DADOS="$work" "$work/feed"
cc "${flags[@]}" tests/seriefrases.c "$work/detail.o" "${objs[@]}" "$OBJ/extras.o" -o "$work/quotes" "${libs[@]}"
NUVIO_DADOS="$work" "$work/quotes"
# Existing detail routing regressions, without rebuilding the application.
all=("${objs[@]}" "$OBJ/extras.o" "$OBJ/seriefrases.o")
cc "${flags[@]}" tests/detail_remonta.c "${all[@]}" "$work/detail.o" -o "$work/remonta" "${libs[@]}"
NUVIO_DADOS="$work" "$work/remonta" > "$work/remonta.log"
! grep -Eq 'catalogo remontou: .* saiu de ([0-9]+) para \1$' "$work/remonta.log"
grep -q '^FIM$' "$work/remonta.log"
bash tests/detail_lower_touch.sh
