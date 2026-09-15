#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Wall -Wextra -Isrc -Wno-macro-redefined -Wno-deprecated-declarations)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# progresso.c so depende de dados.h e perfis.h, e o teste fornece os dois em
# memoria — nada de SDL, disco ou rede.
cc ${flags[@]+"${flags[@]}"} src/progresso.c tests/progresso.c -o /tmp/nuvio-progresso-tests
/tmp/nuvio-progresso-tests
