#!/bin/bash
# "Peca como fala": rota do celular, pedido e resposta do Gemini, escolha no Cinemeta.
set -eu
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/nuvio-peca-test"
cc -Wall -Wextra -O2 -fsanitize=address,undefined tests/peca.c src/peca.c src/js.c src/jsw.c -Isrc -o "$out"
"$out"
rm -f "$out"
