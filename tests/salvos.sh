#!/bin/bash
# Lista local de salvos (salvos.c) sem SDL, sem rede e sem disco de verdade.
#
#   bash tests/salvos.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/salvos.c tests/salvos.c \
  -Isrc -o /tmp/nuvio-salvos-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-salvos-tests
