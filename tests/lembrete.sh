#!/bin/bash
# Lembretes de programa do guia. Ver tests/lembrete.c.
#
#   bash tests/lembrete.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/lembrete.c tests/lembrete.c -Isrc -o /tmp/nuvio-lembrete-tests \
  -O1 -g -Wall -Wextra
/tmp/nuvio-lembrete-tests
