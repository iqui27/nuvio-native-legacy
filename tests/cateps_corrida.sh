#!/bin/bash
# Corrida nas faixas de episodio do catalogo (#203). Ver tests/cateps_corrida.c.
#   bash tests/cateps_corrida.sh                  (ASAN+UBSAN, o padrao)
#   SANITIZE=thread bash tests/cateps_corrida.sh  (TSAN)
#   SANITIZE=0 bash tests/cateps_corrida.sh       (sem sanitizador: so os asserts)
# ASAN por padrao porque sem sanitizador a escrita em heap liberado do codigo
# antigo NAO falha aqui (passa calada e so estoura depois, num free qualquer).
# Medido em 0fcfa601: ASAN aborta com heap-use-after-free (WRITE em
# cat_definir_episodios), TSAN aponta 7 corridas; sem sanitizador passa.
set -eu
cd "$(dirname "$0")/.."
flags=()
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread) ;;
esac
bin="${TMPDIR:-/tmp}/nuvio-cateps-corrida"
cc ${flags[@]+"${flags[@]}"} "${CATALOGO_C:-src/catalogo.c}" src/js.c tests/cateps_corrida.c \
  -Isrc -o "$bin" -O1 -g -Wall -Wno-deprecated-declarations -pthread
"$bin"
