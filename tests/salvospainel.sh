#!/bin/bash
# Painel de Salvos por quadro: reconstrucao so quando a lista muda e o fundo
# parado (a home pintada uma vez num FBO) igual ao desenho direto. Precisa de
# GL (janela escondida); escreve so em NUVIO_DADOS, uma pasta temporaria.
#
#   bash tests/salvospainel.sh
#
# A medida de tempo por quadro e outra: tests/salvospainel_perf.sh.
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-spainel-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/salvospainel.c -Isrc -o "$tmp/teste" \
  -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$tmp/dados" "$tmp/teste" | grep -vE "^\[(tex|arte|cat|desc|home|salvos|txt|jpeg|t|extras)\]|^(reserva|fonte|catalogo: [0-9]+ (ids|episodios|titulos)|home: )"
