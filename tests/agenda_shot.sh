#!/bin/bash
# Capturas da Agenda em BMP, sem interacao e sem rede.
#
#   bash tests/agenda_shot.sh /tmp/nuvio-agenda
#
# Nao entra na suite (tools/testa-tudo.sh pula *_shot.sh): precisa de janela GL
# e de olho humano para julgar.
#
# NUVIO_DADOS aponta para uma pasta temporaria e o .c ABORTA se dados_dir() nao
# for exatamente ela — a captura escreve agenda-p1.txt e lembretes-p1.txt, e um
# teste deste repositorio ja escreveu por cima dos dados reais do dono.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-agenda-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
echo "dados da captura em $NUVIO_DADOS"

sources=()
for source in src/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/agenda_shot.c -Isrc -o /tmp/nuvio-agenda-shot \
  -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-agenda-shot "$@"
