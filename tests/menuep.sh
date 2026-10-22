#!/bin/bash
# O menu do episodio na ilha do menu do cartaz, a aba da temporada que se abre
# em painel e o selo de nota com logo (tests/menuep.c). Pagina de titulo de
# verdade, eventos de tecla e de ponteiro, janela GL escondida do Mac; sem rede.
#
#   bash tests/menuep.sh [prefixo-das-capturas]
#
# Compila tudo MENOS src/main.c e src/detail.c, que o teste inclui (ele le o
# foco e o nivel, estaticos de detail.c), como tests/detail_eps_shot.sh.
set -euo pipefail
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-menuep.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
if [ $# -gt 0 ]; then mkdir -p "$(dirname "$1")"; fi
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/detail.c) continue;; esac
  sources+=("$source")
done
if ! cc "${sources[@]}" tests/menuep.c -Isrc -o "$NUVIO_DADOS/teste" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  >"$NUVIO_DADOS/build.log" 2>&1; then
  cat "$NUVIO_DADOS/build.log" >&2
  exit 1
fi
"$NUVIO_DADOS/teste" "$@"
