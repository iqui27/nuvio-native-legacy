#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Seekr: recorte do quadro dentro da folha, sem rede. Ver tests/seekr_recorte.c.
set -eu
cd "$(dirname "$0")/.."
# Sem SANITIZE: a libSDL2 do brew abre um alerta modal no arranque quando o
# binario tem ASan (medido: dllinit -> error_dialog), e o teste fica parado.
cc src/seekrquota.c src/dados.c src/seekrvtt.c src/js.c src/jpegrapido.c tests/seekr_recorte.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -framework OpenGL -o ${TMPDIR:-/tmp}/nuvio-seekr-recorte -O1 -g \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-seekr-recorte
