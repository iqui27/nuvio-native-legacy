#!/bin/bash
# "O que eu perdi?": corte da legenda sem spoiler, pedido do Gemini e resposta.
set -eu
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/nuvio-perdi-test"
cc -Wall -Wextra -O2 -fsanitize=address,undefined tests/perdi.c src/perdi.c src/js.c src/jsw.c -Isrc -o "$out"
"$out"
rm -f "$out"
