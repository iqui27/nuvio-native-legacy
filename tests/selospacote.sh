#!/bin/bash
# Pacotes de selos (selospacote.c) e o motor de regex (regexjs.c): so os dois
# arquivos, sem GL e sem rede. Disco e perfil sao de mentira, no proprio teste.
#
#   bash tests/selospacote.sh
set -eu
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/nuvio-selospacote"
cc src/regexjs.c src/selospacote.c tests/selospacote.c -Isrc -o "$out" \
  -O1 -g -Wall -Wextra -fsanitize=address,undefined
"$out"
rm -f "$out"
