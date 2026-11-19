#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-menu-perf.XXXXXX")
export NUVIO_DADOS="$work"
export MENU_PERF_OUT="${MENU_PERF_OUT:-/tmp/nuvio-menu-perf-proof}"
mkdir -p "$MENU_PERF_OUT"
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wno-deprecated-declarations -Wno-macro-redefined)
objects=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/menu.c) continue;; esac
  object="$work/$(basename "$source" .c).o"
  cc "${flags[@]}" -c "$source" -o "$object"
  objects+=("$object")
done
# Freeze the task's starting menu; both variants share identical dependencies.
git show 00fd00ba5a37c116eaeacb8d6e81791829186c9c:src/menu.c > "$work/menu-before.c"
for variant in before after; do
  if [ "$variant" = before ]; then
    cc "${flags[@]}" -DMENU_BASELINE "-DMENU_SOURCE=\"$work/menu-before.c\"" -c tests/menu_perf.c -o "$work/harness.o"
  else
    cc "${flags[@]}" -c tests/menu_perf.c -o "$work/harness.o"
  fi
  cc "${objects[@]}" "$work/harness.o" -o "$work/test" \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
  echo "menu-perf variant=$variant"
  "$work/test"
done
cc "${flags[@]}" -c src/menu.c -o "$work/menu.o"
cc "${flags[@]}" -c tests/menu_ocultos.c -o "$work/hidden.o"
cc "${objects[@]}" "$work/menu.o" "$work/hidden.o" -o "$work/hidden" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$work/hidden"
