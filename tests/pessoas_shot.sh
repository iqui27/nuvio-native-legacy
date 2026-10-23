#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas da modal "Encontrar pessoas" (pessoas.c): menu, resultados da busca,
# cartao de perfil, "Meu perfil" e pedidos. BMP, sem rede e sem interacao. Nao
# entra na suite: precisa de janela GL e de olho humano para julgar.
#
#   bash tests/pessoas_shot.sh /tmp/nv-amigos-shots
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-pessoas-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/recomenda.c|src/pessoas.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/pessoas_shot.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-pessoas-shot \
  -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-pessoas-shot "$@"
