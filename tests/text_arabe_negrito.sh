#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-arabe-bold.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/text.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" "${TEST_SOURCE:-tests/text_arabe_negrito.c}" -Isrc -o "$work/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
rc=0
NUVIO_SEM_RESERVA_DE_SISTEMA=1 NUVIO_DADOS="$work" "$work/test" || rc=1
NUVIO_DADOS="$work" "$work/test" || rc=1
exit "$rc"
