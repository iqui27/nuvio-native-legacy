#!/bin/bash
# Paridade da regex do auto-play (padroes com lookahead/lookbehind) com o Nuvio
# OFICIAL: o node roda a logica do NuvioTVSmart (streamAutoPlaySelector.js) e o C
# tem de dar a mesma resposta. Precisa de node; sem ele, pula.
#
#   bash tests/fonteregra_oficial.sh
set -eu
cd "$(dirname "$0")/.."
command -v node >/dev/null 2>&1 || { echo "fonteregra_oficial: sem node, pulado"; exit 0; }
D=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fonteregra-oficial.XXXXXX")
export NUVIO_DADOS="$D"
trap 'rm -rf "$D"' EXIT
node tests/fonteregra_oficial.js > "$D/casos.tsv"
cc -O1 -g -Wall -Wextra -Isrc src/fonteregra.c src/regexjs.c src/dados.c src/js.c tests/fonteregra_oficial.c \
  -o "$D/oficial" -lpthread 2>/dev/null
"$D/oficial" "$D/casos.tsv"
