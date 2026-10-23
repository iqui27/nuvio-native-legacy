#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Codigo de seis caracteres do registro enviado (src/regcodigo.c) igual ao do
# servidor (servidor/recomendacoes/src/codigo.js).
set -eu
cd "$(dirname "$0")/.."
B=$(mktemp ${TMPDIR:-/tmp}/nuvio-regcodigo.XXXXXX)
trap 'rm -f "$B"' EXIT
cc -std=c99 -Wall -Wextra -Werror src/regcodigo.c tests/regcodigo.c -o "$B"
"$B"
