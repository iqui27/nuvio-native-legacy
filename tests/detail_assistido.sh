#!/bin/bash
# #595: opening another title clears a queued watched action.
#   bash tests/detail_assistido.sh
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/detail.c|src/rede.c) continue;; esac
  sources+=("$source")
done
cc -Drede_pedir=nv_base_rede_pedir -c src/rede.c -o "${TMPDIR}/rede-595.o" \
  -Isrc -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined
cc "${sources[@]}" tests/detail_assistido.c "${TMPDIR}/rede-595.o" -Isrc \
  -o "${TMPDIR}/nuvio-detail-assistido-tests" -O1 -g -I/opt/homebrew/include \
  -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf \
  -lz -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
"${TMPDIR}/nuvio-detail-assistido-tests"
