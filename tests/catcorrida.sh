#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Acrescimo ao catalogo com outra troca de bloco entre a entrada e a trava.
#
#   SANITIZE=1 bash tests/catcorrida.sh
#
# Com SANITIZE=1 o ASan acusa heap-buffer-overflow se o tamanho do bloco novo
# for calculado fora de pubTrava (queda "free(): invalid pointer", 1.7.0 .tpk).
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} -DNV_CAT_TEST_ANTES_TRAVA=nv_cat_teste_antes_trava \
  src/catalogo.c tests/catcorrida.c tests/catalogo_boundary_stub.c \
  -Isrc -o ${TMPDIR:-/tmp}/nuvio-catcorrida-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-catcorrida-tests
