#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-origens.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
flags=()
if [ -n "${BIB_REV:-}" ]; then
  git show "$BIB_REV:src/biblioteca.c" > "$tmp/biblioteca.c"
  flags+=("-DBIB_FONTE=\"$tmp/biblioteca.c\"")
fi
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/biblioteca.c) continue;; esac
  sources+=("$source")
done
cc ${flags[@]+"${flags[@]}"} tests/biblioteca_origens.c "${sources[@]}" -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -O0 -g -Wno-deprecated-declarations -Wno-macro-redefined \
  -fsanitize=undefined -fno-sanitize-recover=undefined \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -o "$tmp/teste"
NUVIO_DADOS="$tmp/dados" "$tmp/teste"
