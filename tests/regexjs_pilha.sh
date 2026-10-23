#!/bin/bash
# regexjs sobre repeticao de grupo com texto longo, numa thread de 64 KB de pilha,
# contra o V8. Precisa de node; sem ele, pula. Ver tests/regexjs_pilha.js.
#
#   bash tests/regexjs_pilha.sh
set -eu
cd "$(dirname "$0")/.."
command -v node >/dev/null 2>&1 || { echo "regexjs_pilha: sem node, pulado"; exit 0; }
D=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-regexjs-pilha.XXXXXX")
trap 'rm -rf "$D"' EXIT
node tests/regexjs_pilha.js > "$D/casos.tsv"
cc -O2 -g -Wall -Wextra -Isrc src/regexjs.c tests/regexjs_pilha.c -o "$D/pilha" -lpthread 2>/dev/null
"$D/pilha" "$D/casos.tsv"
