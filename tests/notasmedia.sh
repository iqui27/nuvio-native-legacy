#!/bin/bash
# Media por temporada do cartao "Notas por episodio" (src/notasmedia.c).
#   bash tests/notasmedia.sh
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-notasmedia.XXXXXX")
trap 'rm -rf "$work"' EXIT
extra=()
if [ "${SANITIZE:-0}" = 1 ]; then extra+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/notasmedia.c src/notasmedia.c -Isrc -o "$work/t" ${extra[@]+"${extra[@]}"} -O1 -g -Wall -Wextra -lm 2>&1
saida=$("$work/t")
printf '%s\n' "$saida"
printf '%s\n' "$saida" | grep -q FALHOU && exit 1
echo "notasmedia.sh: ok"
