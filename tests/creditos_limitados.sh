#!/bin/bash
# Sem SDL nem rede: limites explicitos e respostas concorrentes de intro.c.
set -euo pipefail
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
bin=$(mktemp "${TMPDIR:-/tmp}/nuvio-creditos-limitados.XXXXXX")
trap 'rm -f "$bin"' EXIT
cc ${flags[@]+"${flags[@]}"} src/intro.c src/js.c src/credfonte.c tests/creditos_limitados.c \
  -Isrc -o "$bin" -O1 -g -pthread -Wall -Wextra -Wno-deprecated-declarations
"$bin"
