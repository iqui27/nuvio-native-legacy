#!/bin/bash
# Posteres personalizados (src/posterprov.c): URLs de cada provedor, token do
# manifest, redacao, falhas por item, disjuntor e portao. Sem rede.
#
#   bash tests/posterprov.sh
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  src/posterprov.c src/redeurl.c tests/posterprov.c -lpthread -o /tmp/nuvio-posterprov-tests
/tmp/nuvio-posterprov-tests

# #200: o cartaz do item de addon e o da RAIZ do meta, nao o `_providerArt`
# aninhado que o AIOMetadata manda antes dele (tests/posteraddon.c inclui
# descoberta.c, com o conjunto de link de tests/detalheanime.sh).
cc src/catalogo.c src/cwordem.c tests/posteraddon.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/artefontes.c \
  -Isrc -Itests -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o /tmp/nuvio-posteraddon-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-posteraddon-tests
