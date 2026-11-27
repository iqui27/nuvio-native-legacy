#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Cache pelo texto do addon (src/cachemarca.c). Ver tests/cachemarca.c.
#   bash tests/cachemarca.sh
set -eu
cd "$(dirname "$0")/.."
cc src/cachemarca.c src/dobra.c tests/cachemarca.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-cachemarca-tests -O1 -g -std=gnu11 -Wall -Wextra -fsanitize=address,undefined
${TMPDIR:-/tmp}/nuvio-cachemarca-tests
