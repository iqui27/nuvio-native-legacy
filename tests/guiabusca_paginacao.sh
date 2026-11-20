#!/bin/bash
# #344: buscaFazer REAL (src/guia.c incluido) pagina a grade por posicao, sem
# pular nem repetir programa com sobreposicao. Ver tests/guiabusca_paginacao.c.
#
#   bash tests/guiabusca_paginacao.sh
#   SANITIZE=1 bash tests/guiabusca_paginacao.sh
set -euo pipefail
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guiabusca-pag-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for s in src/*.c src/dts/*.c; do case "$s" in src/main.c|src/guia.c) continue;; esac; sources+=("$s"); done
cc ${flags[@]+"${flags[@]}"} "${sources[@]}" tests/guiabusca_paginacao.c -Isrc -o "$tmp/t" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$tmp" "$tmp/t"
