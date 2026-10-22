#!/bin/bash
# Avisos dispensados: o arquivo por perfil (tests/avisodisp.c) e a central de
# avisos usando ele (tests/avisodisp_avisos.c). Sem janela nem rede.
#
#   bash tests/avisodisp.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-avisodisp-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/avisodisp.c src/dados.c tests/avisodisp.c -Isrc -o "$tmp/unidade" \
  -O1 -g -Wall -Wextra
mkdir -p "$tmp/nuvio-avisodisp-1"
NUVIO_DADOS="$tmp/nuvio-avisodisp-1" "$tmp/unidade" | grep -v "^\[dados\]"
# A central inteira: precisa do resto do app linkado (sem janela).
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/avisodisp_avisos.c -Isrc -o "$tmp/central" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -w
mkdir -p "$tmp/nuvio-avisodisp-2"
NUVIO_DADOS="$tmp/nuvio-avisodisp-2" NUVIO_AVISOS_DEMO=1 "$tmp/central" | grep -E "^(avisodisp|\[avisos\] dispensado)"
