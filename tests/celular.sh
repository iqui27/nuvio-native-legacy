#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Servidor "Digitar pelo celular" contra curl de verdade. Ver tests/celular.c.
#   bash tests/celular.sh
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-celular-XXXXXX")"
trap 'rm -rf "$tmp" ${TMPDIR:-/tmp}/nv-celular-corpo.txt ${TMPDIR:-/tmp}/nv-celular-grande.txt' EXIT
flags=(-O1 -g -Isrc -Wall -Wextra -Wno-unused-parameter)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/celular.c tests/celular.c -o "$tmp/celular" -lpthread
"$tmp/celular"
