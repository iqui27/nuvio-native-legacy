#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# P2P experimental (src/p2p.c): endereco, corpo do create, escolha de arquivo,
# parse de infoHash/fileIdx/sources e o mapeamento de erro do resolvedor, tudo
# com a rede FALSA (tests/p2p.c). Nao precisa de servidor Stremio.
#
#   bash tests/p2p.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  src/p2p.c src/p2pmotor.c src/p2pmotor_motor.c src/stream_parse.c src/fontefiltro.c src/linguas.c src/js.c tests/p2p.c -o ${TMPDIR:-/tmp}/nuvio-p2p-tests
${TMPDIR:-/tmp}/nuvio-p2p-tests
