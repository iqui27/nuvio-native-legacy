#!/bin/bash
# Prioridade metahub > Cinemeta na arte do enfeite. Sem rede.
#
#   bash tests/arte_metahub.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} tests/arte_metahub.c \
  -Isrc -o /tmp/nuvio-arte-metahub-tests -O1 -g \
  -Wall -Wextra -Wno-deprecated-declarations
/tmp/nuvio-arte-metahub-tests
