#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas do cartao da 1.8.0 (PNG) nos quadros do mockup aprovado, depois das
# regras por tecla. Janela GL escondida, desenho num FBO. Fica fora da suite
# (*_shot.sh): precisa de GL e de olho humano.
#
#   bash tests/novidades180_shot.sh /tmp/nuvio-n180              # vidro
#   NUVIO_SHOT_VIDRO=0 bash tests/novidades180_shot.sh /tmp/n180  # solido
#   N180_SO=peek bash tests/novidades180_shot.sh                  # so os quadros com "peek"
#   N180_EN=1 ...                                                 # e em ingles e russo
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-n180-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS" ${TMPDIR:-/tmp}/nuvio-n180-shot' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in */main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/novidades180_shot.c -Isrc \
  -o ${TMPDIR:-/tmp}/nuvio-n180-shot -O1 \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-n180-shot "${1:-${TMPDIR:-/tmp}/nuvio-novidades180}"
