#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# "A seguir" do Trakt respeita o Trakt (issue #213). Sem rede.
set -eu
cd "$(dirname "$0")/.."
cc tests/traktult.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-traktult-tests -O1 -g -Wall
${TMPDIR:-/tmp}/nuvio-traktult-tests
# trakt.c usa a regra de empate e consulta o next_episode do proprio Trakt.
grep -q 'tk_ult_anotar(ult, &nUlt, TK_ULT_MAX' src/trakt.c tests/stub_fichameta.c
grep -q '/progress/watched' src/trakt.c
grep -q 'tk_prog_proximo(corpo' src/trakt.c
echo "traktult: trakt.c usa empate e next_episode"
