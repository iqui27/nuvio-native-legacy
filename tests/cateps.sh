#!/bin/bash
# Faixas de episodio contra o catalogo que cresce.
#
#   bash tests/cateps.sh
#
# So src/catalogo.c, como tests/catfileira.sh: a regra e sobre indices e nao
# depende de SDL, de rede nem da descoberta.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# cateps.c inclui descoberta.c inteiro (pelo desc_tmdb_notas_temporada, pura),
# entao o link precisa do mesmo conjunto de tests/colfileiras.sh mais o
# catalogo.c de verdade — que e o objeto do teste original.
cc ${flags[@]+"${flags[@]}"} src/catalogo.c tests/cateps.c \
  src/js.c src/colecoes.c src/redeurl.c src/catordem.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o /tmp/nuvio-cateps-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-cateps-tests
