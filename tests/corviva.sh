#!/bin/bash
# Cor viva (tema "Dinâmica"): extracao, contraste, debounce e interpolacao.
# Ver tests/corviva.c.
set -eu
cd "$(dirname "$0")/.."
cc -Wall -Wextra -O2 tests/corviva.c src/corviva.c -Isrc -o /tmp/nuvio-corviva-test -lm
/tmp/nuvio-corviva-test
