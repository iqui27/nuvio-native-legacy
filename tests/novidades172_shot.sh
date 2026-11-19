#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas do cartao da 1.7.2 em BMP: as tres cenas da previa (cara nova,
# entrar com e-mail, fontes na hora) em pt, en e ja, e as regras de tecla.
# Janela GL escondida e desenho num FBO. Fora da suite (*_shot.sh).
#
#   bash tests/novidades172_shot.sh /tmp/nuvio-novidades172
#   N172_SO=ja bash tests/novidades172_shot.sh      # so um grupo de capturas
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-n172-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in */main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/novidades172_shot.c -Isrc \
  -o ${TMPDIR:-/tmp}/nuvio-n172-shot -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-n172-shot "${1:-${TMPDIR:-/tmp}/nuvio-novidades172}"
