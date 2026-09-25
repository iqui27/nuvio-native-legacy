#!/bin/bash
# Capturas da tela "Entre amigos" (social.c) em BMP, sem rede e sem interacao.
# Nao entra na suite: precisa de janela GL e de olho humano para julgar.
#
#   bash tests/social_tela_shot.sh /tmp/nuvio-social-tela
#   NUVIO_RAIL=fixa bash tests/social_tela_shot.sh /tmp/nuvio-social-fixa
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-social-tela.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/social.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/social_tela_shot.c -Isrc -o /tmp/nuvio-social-tela-shot \
  -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-social-tela-shot "$@"
