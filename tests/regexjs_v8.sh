#!/bin/bash
# O motor de regex proprio (regexjs.c) e o filtro do auto-play (fonteregra.c)
# contra o V8: ~23 mil casos aleatorios de motor e regex de ~400 caracteres sobre
# textos de fonte de 200 a 8000 bytes, com teto de tempo. Precisa de node; sem
# ele, pula. Ver tests/regexjs_v8.js.
#
#   bash tests/regexjs_v8.sh
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Isrc src/regexjs.c tests/regexjs_pr489.c -o "${TMPDIR:-/tmp}/regexjs-pr489.$$" -lpthread
"${TMPDIR:-/tmp}/regexjs-pr489.$$"; rm -f "${TMPDIR:-/tmp}/regexjs-pr489.$$"
command -v node >/dev/null 2>&1 || { echo "regexjs_v8: sem node, pulado"; exit 0; }
D=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-regexjs-v8.XXXXXX")
export NUVIO_DADOS="$D"
trap 'rm -rf "$D"' EXIT
node tests/regexjs_v8.js > "$D/casos.tsv"
cc -O2 -g -Wall -Wextra -Isrc src/fonteregra.c src/regexjs.c src/dados.c src/js.c tests/regexjs_v8.c \
  -o "$D/v8" -lpthread 2>/dev/null
"$D/v8" "$D/casos.tsv"
