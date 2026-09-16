#!/bin/bash
# Capturas das tres paginas do cartao de primeira vez do Social, em BMP, sem
# interacao. Nao entra na suite: precisa de janela GL e de olho humano.
#
#   bash tests/recintro_shot.sh /tmp/nuvio-recintro
#
# src/recomenda.c e INCLUIDO pelo teste (como em tests/social_shot.sh) para que
# NV_REC_URL fique definida: sem ela recomenda_ativo() e 0 e o cartao nao abre —
# que e o comportamento certo do pacote sem servico, e nao o que esta foto quer
# provar.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-recintro-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
echo "dados da captura em $NUVIO_DADOS"

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/recomenda.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/recintro_shot.c -Isrc -o /tmp/nuvio-recintro-shot \
  -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-recintro-shot "$@"
