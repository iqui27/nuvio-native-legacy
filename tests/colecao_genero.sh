#!/bin/bash
# Chip de genero na pagina de colecao (issue #656).
set -euo pipefail
cd "$(dirname "$0")/.."
sources=()
flags=(-O1 -g)
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/vertudo.c) continue;; esac
  sources+=("$source")
done
bin="$(mktemp "${TMPDIR:-/tmp}/nuvio-colecao-genero.XXXXXXXX")"
trap 'rm -f "$bin"' EXIT
"${CC:-cc}" "${sources[@]}" tests/colecao_genero.c -Isrc -o "$bin" "${flags[@]}" \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
"$bin"
