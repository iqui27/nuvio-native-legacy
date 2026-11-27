#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Fontes sob demanda: nomes da tabela 'name', familias do documento ASS e a
# decisao de entregar cada fonte anexada. Ver tests/fonte_familias.c.
set -eu
cd "$(dirname "$0")/.."
cc -Wall -Wextra -Isrc tests/fonte_familias.c src/fontefamilias.c -o "$TMPDIR/nuvio-fonte-familias"
"$TMPDIR/nuvio-fonte-familias"
