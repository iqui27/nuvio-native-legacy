#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas do seletor de idioma e de telas em idiomas de escrita nova (grego,
# vietnamita, japones, chines). Nao entra na suite: precisa de janela GL e de olho
# humano para julgar. Ver tests/idioma_shot.c.
#
#   bash tests/idioma_shot.sh /tmp/nuvio-idioma-shots/x 27,28,29,24,26
#   NUVIO_SEM_RESERVA_DE_SISTEMA=1 bash tests/idioma_shot.sh ...   # o que o WASM desenha
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc -DAJUSTES_TESTE "${sources[@]}" tests/idioma_shot.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-idioma-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-idioma-shot "$@"
