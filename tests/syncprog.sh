#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Wall -Wextra -Isrc -Wno-macro-redefined -Wno-deprecated-declarations)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# syncprog.c fala com a conta por sessao_rpc e com o app por progresso.c e
# catalogo.c; o teste fornece sessao_rpc, dados_*, perfis_ativo e as duas
# funcoes do catalogo. js.c/jsw.c sao os de verdade — o contrato e o texto.
cc ${flags[@]+"${flags[@]}"} src/syncprog.c src/progresso.c src/js.c src/jsw.c tests/syncprog.c -o /tmp/nuvio-syncprog-tests
/tmp/nuvio-syncprog-tests
