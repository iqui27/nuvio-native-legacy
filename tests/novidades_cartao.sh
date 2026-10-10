#!/bin/bash
# O cartao de novidades da versao atual (novidades_cartao.h): abre uma vez por
# versao, grava a marca, nunca por cima do player ou da pagina do titulo, e os
# cartoes da 2.0.1/2.0.2 nunca abrem depois dele. Sem janela nem rede.
#   bash tests/novidades_cartao.sh
set -euo pipefail
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-novcartao.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/novidades_cartao.c -Isrc -o "$NUVIO_DADOS/teste" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$NUVIO_DADOS/teste" "$@"
