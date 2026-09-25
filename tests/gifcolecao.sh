#!/bin/bash
# O GIF do cartaz de colecao em foco: capa GIF como fonte, motivo de nao
# animar, URL saneada e troca de foco (#141). Ver tests/gifcolecao.c.
#
#   bash tests/gifcolecao.sh
#   SANITIZE=1 bash tests/gifcolecao.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/gifcolecao.c src/gif.c tests/gifcolecao.c \
  -Isrc -o /tmp/nuvio-gifcolecao-tests -O1 -g -pthread \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-gifcolecao-tests
