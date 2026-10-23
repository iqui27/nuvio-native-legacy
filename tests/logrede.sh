#!/bin/bash
# logrede_limpar: a pagina de log na rede de casa nunca mostra credencial,
# URL de addon/debrid ou titulo (PR #640). So a parte portatil do arquivo.
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-logrede.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
cc -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -D__EMSCRIPTEN__ \
  -Isrc src/logrede.c tests/logrede.c -o "$DIR/teste"
"$DIR/teste"
