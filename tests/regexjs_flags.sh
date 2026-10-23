#!/bin/bash
# regexjs com as flags s e m contra o V8 (otimizacoes de primeiro byte e de "^"
# incluidas). Precisa de node; sem ele, pula. Ver tests/regexjs_flags.js.
#
#   bash tests/regexjs_flags.sh
set -eu
cd "$(dirname "$0")/.."
command -v node >/dev/null 2>&1 || { echo "regexjs_flags: sem node, pulado"; exit 0; }
D=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-regexjs-flags.XXXXXX")
trap 'rm -rf "$D"' EXIT
node tests/regexjs_flags.js > "$D/casos.tsv"
cc -O2 -g -Wall -Wextra -Isrc src/regexjs.c tests/regexjs_flags.c -o "$D/flags" 2>/dev/null
"$D/flags" "$D/casos.tsv"
