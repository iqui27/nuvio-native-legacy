#!/bin/bash
# Capturas do cartao de novidades da 1.1 em BMP, sem interacao e sem rede.
#
#   bash tests/novidades11_shot.sh /tmp/nuvio-n11
#
# Nao entra na suite (tools/testa-tudo.sh pula *_shot.sh): precisa de janela GL
# e de olho humano para julgar.
#
# NUVIO_DADOS aponta para uma pasta temporaria e o .c ABORTA se dados_dir() nao
# for exatamente ela — fechar o cartao GRAVA novidades-11.txt, e um teste deste
# repositorio ja escreveu por cima dos dados reais do dono.
#
# TABELA DE TRADUCAO DE RASCUNHO. As chaves novas ainda nao estao em
# src/idioma_tab.h (elas sao entregues a parte para o dono fundir), entao as
# capturas -en sairiam em portugues e nao provariam nada sobre o ingles.
#
# COM NUVIO_TAB a arvore src/ INTEIRA e copiada para uma pasta temporaria e so
# la o idioma_tab.h e trocado. Nao da para resolver isto com -I: idioma.c pede
# #include "idioma_tab.h" entre ASPAS, e um include entre aspas procura primeiro
# a pasta do proprio arquivo que inclui — src/ sempre venceria a pasta do -I, e
# a captura -en sairia em portugues dizendo que estava tudo bem. Foi o que
# aconteceu na primeira rodada. O arquivo do repositorio nunca e tocado.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-n11-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
echo "dados da captura em $NUVIO_DADOS"

dirSrc=src
if [ -n "${NUVIO_TAB:-}" ]; then
  dirSrc=$(mktemp -d /tmp/nuvio-n11-src.XXXXXX)
  trap 'rm -rf "$NUVIO_DADOS" "$dirSrc"' EXIT
  cp src/*.c src/*.h "$dirSrc"/
  cp "$NUVIO_TAB" "$dirSrc/idioma_tab.h"
  echo "tabela de traducao de rascunho: $NUVIO_TAB (arvore em $dirSrc)"
fi

sources=()
for source in "$dirSrc"/*.c; do
  case "$source" in */main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/novidades11_shot.c -I"$dirSrc" \
  -o /tmp/nuvio-n11-shot \
  -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-n11-shot "$@"
