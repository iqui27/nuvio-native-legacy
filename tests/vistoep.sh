#!/bin/bash
# Mapa de episodios vistos. Sem SDL, sem rede: so vistoep.c e o leitor de JSON.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/vistoep.c src/js.c tests/vistoep.c \
  -Isrc -o /tmp/nuvio-vistoep-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-vistoep-tests
