#!/bin/bash
# Render focus targets and exercise the public Library handler in TV and phone layouts.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-biblioteca-up.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=()
if [ -n "${BIB_UP_REV:-}" ]; then
  git show "$BIB_UP_REV:src/biblioteca.c" > "$work/biblioteca.c"
  flags+=("-DBIB_UP_SOURCE=\"$work/biblioteca.c\"")
fi
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/app.c|src/biblioteca.c) continue;; esac
  sources+=("$source")
done
cc -DNV_TOUCH_UI "${flags[@]+${flags[@]}}" -O1 -g tests/biblioteca_up.c tests/biblioteca_up_router.c "${sources[@]}" -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined -o "$work/test"
NUVIO_DADOS="$work/dados" "$work/test"
