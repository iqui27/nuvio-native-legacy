#!/bin/bash
# Live TV por fonte (src/ltv.c): arena, teto, grupos, numeracao, M3U em fluxo.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} -Wall -Wextra -Wno-deprecated-declarations src/ltv.c tests/ltv.c -lpthread -Isrc -o /tmp/nuvio-ltv-tests -O1 -g
/tmp/nuvio-ltv-tests
