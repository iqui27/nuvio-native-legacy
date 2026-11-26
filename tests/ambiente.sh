#!/bin/bash
# Tela ambiente ao pausar: regra pura de entrada da arte (src/ambiente.c).
#   bash tests/ambiente.sh
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ambiente.XXXXXX")
trap 'rm -rf "$work"' EXIT
extra=()
if [ "${SANITIZE:-0}" = 1 ]; then extra+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/ambiente.c src/ambiente.c -Isrc -o "$work/t" ${extra[@]+"${extra[@]}"} -O1 -g -Wall -Wextra -lm 2>&1
saida=$("$work/t")
printf '%s\n' "$saida"
printf '%s\n' "$saida" | grep -q FALHOU && exit 1
echo "ambiente.sh: ok"
