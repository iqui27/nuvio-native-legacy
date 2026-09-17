#!/bin/bash
# Cartao de atualizacao nos quatro estados, em BMP. Nao entra na suite
# (tools/testa-tudo.sh pula *_shot.sh): precisa de janela GL e de olho humano.
#
#   bash tests/atualizacao_shot.sh /tmp/nuvio-update
#
# NV_AT_INSTALA=1 encena a TV COM Homebrew Channel, que e o ramo com botoes e
# barra de progresso. Nenhum build de produto define esta macro.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-update-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/atualizacao.c) continue;; esac
  sources+=("$source")
done
# tools/env.sh injeta -DNV_VERSAO a partir do appinfo.json: sem ele a linha
# "Voce esta na X" sai como "dev", que nao e o que a TV mostra.
ENV_D=$(tools/env.sh)
eval cc "${sources[@]}" tests/atualizacao_shot.c -Isrc -o /tmp/nuvio-update-shot \
  -O1 -g -DNV_AT_INSTALA=1 "$ENV_D" \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-update-shot "$@"
