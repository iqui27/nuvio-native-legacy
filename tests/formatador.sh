#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Icones dos formatadores de fonte (src/formatador.c). Ver tests/formatador.c.
#   bash tests/formatador.sh
set -eu
cd "$(dirname "$0")/.."
cc src/formatador.c src/limpa.c src/dobra.c tests/formatador.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-formatador-tests -O1 -g -std=c11 -Wall -Wextra -fsanitize=address,undefined
${TMPDIR:-/tmp}/nuvio-formatador-tests tools/icones-formatador.tsv
