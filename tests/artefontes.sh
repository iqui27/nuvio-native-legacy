#!/bin/bash
# Leitura das respostas das fontes de arte do destaque (src/artefontes.c) e a
# escolha na busca da Apple (trailerapple.c), contra respostas REAIS gravadas
# em tests/fixtures/arte (TMDB, Kitsu, AniList e Apple por curl em 23/09; a
# do fanart.tv e do formato documentado da v3, sem chave para gravar uma).
# Sem rede.
#   bash tests/artefontes.sh
set -eu
cd "$(dirname "$0")/.."
cc -Isrc tests/artefontes.c src/artefontes.c src/js.c -o /tmp/nuvio-artefontes -O1 -g -Wall -Wextra
/tmp/nuvio-artefontes tests/fixtures/arte
cc -O1 -g -Wall -Wno-unused-function -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  tests/trailerapple-arte.c src/js.c -L/opt/homebrew/lib -lSDL2 -lpthread -o /tmp/nuvio-trailerapple-arte
/tmp/nuvio-trailerapple-arte tests/fixtures/arte
