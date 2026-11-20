#!/bin/bash
# #768: probe de arranque — o valor salvo tem de sobreviver ao reinicio.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -DAJUSTES_TESTE=1 -Isrc tests/barras_arranque.c -o "${TMPDIR:-/tmp}/nv-barras-arranque"
"${TMPDIR:-/tmp}/nv-barras-arranque"
echo "barras_arranque: ok"
