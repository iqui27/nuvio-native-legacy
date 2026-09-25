#!/bin/bash
# Capturas da tela de Ajustes e da folha de fileiras, em BMP, sem interacao.
# Nao entra na suite: precisa de janela GL e de olho humano para julgar.
#
#   bash tests/ajustes_shot.sh /tmp/nuvio-ajustes-antes
#   NUVIO_RAIL=fixa bash tests/ajustes_shot.sh /tmp/nuvio-ajustes-fixa
#
# O roteiro (tests/ajustes_shot.c) segue a navegacao da arquitetura do web:
# indice de categorias -> lista -> grupo. Alem das telas de sempre, grava
# Aparencia com as linhas da cor (-aparencia-cor), Avancado > Diagnostico com o
# teste de velocidade (-avancado-velocidade), a abertura pelo cartao de
# novidades (-abrir-na-cor) e TODAS as linhas, categoria a categoria e grupo a
# grupo (-todas-c<N>[-g<M>]-<pagina>).
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/ajustes_shot.c -Isrc -o /tmp/nuvio-ajustes-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-ajustes-shot "$@"
