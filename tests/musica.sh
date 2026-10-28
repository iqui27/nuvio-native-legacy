#!/bin/bash
# Musicas: parse da resposta do Worker, URL do QR e lista Guardadas por perfil.
set -eu
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/nuvio-musica-test"
cc -Wall -Wextra -O2 -DNV_REC_URL='""' tests/musica.c src/musica.c src/js.c -Isrc -o "$out" -lpthread
"$out"
rm -f "$out"
cc -Wall -Wextra -O1 -ffunction-sections -fdata-sections -DNV_MUSICA_SHOT \
  tests/musica_preview.c src/musica.c src/js.c -Isrc -I/opt/homebrew/include \
  -I/opt/homebrew/include/SDL2 -Wl,-dead_strip -framework OpenGL -lpthread -o "$out"
"$out"
rm -f "$out"
