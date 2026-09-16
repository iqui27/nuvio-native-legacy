#!/bin/bash
# Capturas da PAGINA DE TITULO com as duas secoes novas (audiencia e frases),
# em PNG, sem rede e sem interacao. Nao entra na suite (testa-tudo.sh pula
# *_shot.sh): precisa de janela GL e de olho humano para julgar.
#
#   bash tests/detail_secoes_shot.sh /tmp/nuvio-detsec
#
# Compila tudo MENOS src/main.c e src/detail.c, que a captura inclui — ela
# precisa do foco, do nivel e da rolagem, que sao estaticos de detail.c, e
# intercepta extras.h por #define para nao depender de rede nenhuma.
#
# NUVIO_DADOS APONTA PARA UMA PASTA TEMPORARIA: a captura ESCREVE o cache de
# serieaud e de seriefrases, e o proprio programa se recusa a rodar se
# dados_dir() nao for ela.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-detsec-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
echo "dados do teste em $NUVIO_DADOS"

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/detail.c) continue;; esac
  sources+=("$source")
done

cc "${sources[@]}" tests/detail_secoes_shot.c -Isrc -o /tmp/nuvio-detsec-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-detsec-shot "$@"
