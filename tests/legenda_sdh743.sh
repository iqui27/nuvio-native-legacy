#!/bin/bash
# #743: "Evitar legendas SDH" — cobertura do mecanismo de decisao (linguas.c).
#   bash tests/legenda_sdh743.sh
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-sdh743.XXXXXX")
trap 'rm -rf "$work"' EXIT
extra=()
if [ "${SANITIZE:-0}" = 1 ]; then extra+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/legenda_sdh743.c src/linguas.c -Isrc -o "$work/t" ${extra[@]+"${extra[@]}"} \
  -O1 -g -lm -Wno-deprecated-declarations -Wno-macro-redefined 2>&1
saida=$("$work/t")
printf '%s\n' "$saida"
printf '%s\n' "$saida" | grep -q FALHOU && exit 1
echo "legenda_sdh743.sh: ok"
