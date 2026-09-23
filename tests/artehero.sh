#!/bin/bash
# Politica de arte de TELA CHEIA. Sem SDL, sem rede e sem disco: artehero.c so
# olha a url que o catalogo guarda e devolve outra string.
#
# Roda DUAS vezes: a segunda com -D__EMSCRIPTEN__, que e o ramo da Samsung em
# fundoOriginal() — la nenhuma fonte pode pedir fundo `original` (OOM do
# registro 1450), e so compilando o ramo se prova isso.
#
#   bash tests/artehero.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/artehero.c tests/artehero.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o /tmp/nuvio-artehero-tests -O1 -g \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-artehero-tests
cc ${flags[@]+"${flags[@]}"} -D__EMSCRIPTEN__ src/artehero.c tests/artehero.c \
  -Isrc -o /tmp/nuvio-artehero-tests-tizen -O1 -g \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-artehero-tests-tizen
