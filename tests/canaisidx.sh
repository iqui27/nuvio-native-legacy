#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# #37: canal clicado abria outro (colisao em cat_indice_por_imdb por causa do
# corte em ':' pensado so para temporada:episodio de IMDb). Ver tests/canaisidx.c.
#
#   bash tests/canaisidx.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} ${NUVIO_CFLAGS:-} src/catalogo.c src/tendencia.c tests/canaisidx.c tests/catalogo_boundary_stub.c \
  -Isrc -o ${TMPDIR:-/tmp}/nuvio-canaisidx-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-canaisidx-tests
