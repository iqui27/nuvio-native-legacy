#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Nuvio Casa (src/casa.c, src/casa_rede.c): a regra pura e a ponta a ponta em
# 127.0.0.1 com o p2p.c de verdade e um motor falso. Nao precisa de rede.
#
#   bash tests/casa.sh             SANITIZE=1 bash tests/casa.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} -O1 -g -Wall -Wextra -Isrc src/casa.c tests/casa.c -o "$TMPDIR/nuvio-casa-tests"
"$TMPDIR/nuvio-casa-tests"
cc ${flags[@]+"${flags[@]}"} -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  src/casa.c src/casa_rede.c src/p2p.c src/p2pmotor.c src/p2pmotor_motor.c src/stream_parse.c \
  src/fontefiltro.c src/linguas.c src/js.c tests/casa_ponta.c -lpthread -o "$TMPDIR/nuvio-casa-ponta"
"$TMPDIR/nuvio-casa-ponta"
