#!/bin/bash
# Home e pagina de titulo com o tema "Dinâmica" e "Dinâmica estilizada", em BMP,
# sem rede (artes de deploy/app/art). Nao entra na suite (testa-tudo.sh pula
# *_shot.sh): precisa de janela GL e de olho humano.
#
#   bash tests/corviva_shot.sh /tmp/nuvio-corviva
#   sips -s format png /tmp/nuvio-corviva-1-home-dinamica-vermelho.bmp --out x.png
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-corviva-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/corviva_shot.c -Isrc -o /tmp/nuvio-corviva-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined 2>&1 \
  | grep -E "corviva_shot|error" || true
/tmp/nuvio-corviva-shot "$@"
