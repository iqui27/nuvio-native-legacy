#!/bin/bash
# Xtream Codes (src/xtream.c): cadastro por perfil, lista de canais e URL de
# reproducao, com rede e disco dublados. Ver tests/xtream.c.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} ${NUVIO_CFLAGS:-} src/xtream.c src/js.c tests/xtream.c \
  -Isrc -o /tmp/nuvio-xtream-tests -O1 -g -Wall -Wextra -lpthread \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-xtream-tests
