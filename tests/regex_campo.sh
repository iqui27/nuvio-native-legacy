#!/bin/bash
# O campo "Padrão do filtro" de ponta a ponta (texto do celular com ñ e
# bandeiras, regex de 400-500 caracteres, apagar em UTF-8). Compila o app
# inteiro com -DAJUSTES_TESTE, sem janela. Ver tests/regex_campo.c.
#
#   bash tests/regex_campo.sh
set -euo pipefail
cd "$(dirname "$0")/.."
export NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-regexcampo-dados.XXXXXX")
BIN="$NUVIO_DADOS/regex_campo"
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
if [ "$(uname)" = Darwin ]; then
  LIBS=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib
        -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
else
  # shellcheck disable=SC2207
  LIBS=($(pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image) -lGL -lz -lpthread -lm)
fi
cc -DAJUSTES_TESTE "${sources[@]}" tests/regex_campo.c -Isrc -o "$BIN" -O1 -g \
  "${LIBS[@]}" -Wno-deprecated-declarations -w
"$BIN"
