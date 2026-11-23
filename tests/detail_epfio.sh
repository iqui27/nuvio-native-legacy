#!/bin/bash
# #F110: exit() com o fio de episodios em voo dentro da libcurl/OpenSSL.
#   bash tests/detail_epfio.sh
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/detail.c|src/rede.c) continue;; esac
  sources+=("$source")
done
cc -Drede_pedir=nv_base_rede_pedir -c src/rede.c -o "${TMPDIR}/rede-f110.o" \
  -Isrc -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined
cc "${sources[@]}" tests/detail_epfio.c "${TMPDIR}/rede-f110.o" -Isrc \
  -o "${TMPDIR}/nuvio-detail-epfio-tests" -O1 -g -I/opt/homebrew/include \
  -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf \
  -lz -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
# A falha e na saida do processo e probabilistica: repete para nao passar batido.
for i in 1 2 3 4 5; do
  "${TMPDIR}/nuvio-detail-epfio-tests"
done
