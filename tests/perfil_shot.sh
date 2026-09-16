#!/bin/bash
# Capturas da tela "Perfil e Stats", em BMP, sem rede e sem conta do Trakt.
# NAO entra na suite: precisa de janela GL e de olho humano para julgar — mesma
# regra de tests/ajustes_shot.sh e tests/perfilsel.sh --capturas.
#
#   bash tests/perfil_shot.sh [prefixo]      (padrao: /tmp/nuvio-perfil)
set -eu
cd "$(dirname "$0")/.."
fontes=()
for f in src/*.c; do [ "$f" != src/main.c ] && fontes+=("$f"); done
cc "${fontes[@]}" tests/perfil_shot.c -Isrc -o /tmp/nuvio-perfil-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined

# PASTA DESCARTAVEL. NUVIO_DADOS e a unica variavel que dados_iniciar()
# respeita; NUVIO_TESTE_DIR e o que o binario confere antes de desenhar. Sem as
# duas ele se recusa a rodar, e e de proposito: um teste grafico ja apagou o
# ~/.nuvio de quem o rodou uma vez.
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" /tmp/nuvio-perfil-shot "${1:-/tmp/nuvio-perfil}"
