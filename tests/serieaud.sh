#!/bin/bash
# Audiencia da serie: /stats, retencao (com a armadilha do E2 > E1), buracos na
# temporada, teto de pedidos, abandono e o cache de disco sobrevivendo a um
# arranque.
#
#   bash tests/serieaud.sh
#
# Compila tudo MENOS src/serieaud.c, que o teste inclui — o parser e o fio sao
# estaticos (mesma receita de tests/recomenda.sh).
#
# NUVIO_DADOS APONTA PARA UMA PASTA TEMPORARIA, e o proprio teste se recusa a
# rodar se dados_dir() nao for ela. Este teste ESCREVE serieaud-*.txt; sem esta
# linha ele escreveria dentro do ~/.nuvio de quem o executa.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-serieaud-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
echo "dados do teste em $NUVIO_DADOS"

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/serieaud.c) continue;; esac
  sources+=("$source")
done

cc "${sources[@]}" tests/serieaud.c -Isrc -o /tmp/nuvio-serieaud \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-serieaud
