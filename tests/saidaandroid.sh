#!/bin/bash
# Motivo de saida do Android que nao e queda (aviso de crash e modo seguro).
#
#   bash tests/saidaandroid.sh
set -eu
cd "$(dirname "$0")/.."
cc src/saidaandroid.c tests/saidaandroid.c -Isrc -o "${TMPDIR:-/tmp}/nuvio-saidaandroid-tests" -O1 -g -Wall -Wextra
"${TMPDIR:-/tmp}/nuvio-saidaandroid-tests"
