#!/bin/bash
# Ponteiro do Magic Remote (src/ponteiro.c, issue #99): hit-test, camadas,
# hover, clique = OK, rodinha, seta que esconde e os avisos 484/485.
#
#   bash tests/ponteiro.sh
set -eu
cd "$(dirname "$0")/.."
bin=$(mktemp "${TMPDIR:-/tmp}/nuvio-ponteiro.XXXXXX")
trap 'rm -f "$bin"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/ponteiro.c src/ponteiro.c -Isrc -o "$bin" -Wall -Wextra \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined ${flags[@]+"${flags[@]}"}
"$bin"
