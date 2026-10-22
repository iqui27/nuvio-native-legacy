#!/bin/bash
# Sinopse dos candidatos do destaque (tests/herosinopse.c). Mesmo conjunto de
# link de tests/detalheanime.sh, de onde o teste herda os dubles.
#
#   bash tests/herosinopse.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/cwordem.c tests/herosinopse.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/artefontes.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o /tmp/nuvio-herosinopse-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-herosinopse-tests
