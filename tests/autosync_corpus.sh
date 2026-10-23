#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Corpus sintetico do AutoSync (tests/autosync_corpus.c): tabela da engine
# atual e, com ANTES=1, a mesma tabela da engine do v2.0.0 (git show).
# RODADAS=n muda as sementes por caso (padrao 8). Falha se a engine atual
# aplicar errado em qualquer rodada.
set -euo pipefail
cd "$(dirname "$0")/.."
D=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-ascorpus.XXXXXX)
trap 'rm -rf "$D"' EXIT
R=${RODADAS:-8}
cc -Isrc -O2 -Wall -Wextra src/autosync.c src/legenda.c src/assrender.c tests/autosync_corpus.c \
  -pthread -lm -o "$D/novo"
if [ "${ANTES:-0}" = 1 ]; then
  mkdir -p "$D/v200"
  git show v2.0.0:src/autosync.c > "$D/v200/autosync.c"
  git show v2.0.0:src/autosync.h > "$D/v200/autosync.h"
  cc -I"$D/v200" -Isrc -O2 -DAS_ANTIGO "$D/v200/autosync.c" src/legenda.c src/assrender.c \
    tests/autosync_corpus.c -pthread -lm -o "$D/antigo"
  echo "== BEFORE (v2.0.0 engine) =="
  "$D/antigo" "$R" ${VERBOSO:+v}
  echo
fi
echo "== AFTER (current engine) =="
"$D/novo" "$R" ${VERBOSO:+v} | tee "$D/saida"
grep -q '^TOTAL wrong=0 refused=0$' "$D/saida"
