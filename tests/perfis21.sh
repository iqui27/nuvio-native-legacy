#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# 2.1: criar, editar e remover perfis da conta (perfis.c + o modal que chama).
# Sem SDL, sem rede: o "servidor" e o do tests/perfis21.c.
#
#   bash tests/perfis21.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/perfis.c src/js.c src/jsw.c tests/perfis21.c \
  -Isrc -o ${TMPDIR:-/tmp}/nuvio-perfis21-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined

DIR=$(mktemp -d); trap 'rm -rf "$DIR"' EXIT
NUVIO_TESTE_DIR="$DIR" ${TMPDIR:-/tmp}/nuvio-perfis21-tests
echo "perfis21: tudo ok"
