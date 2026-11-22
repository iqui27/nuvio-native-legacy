#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# A Sessao pela rede local contra curl de verdade. Ver tests/sala_celular.c.
#   bash tests/sala_celular.sh
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "$TMPDIR/nuvio-sala-celular-XXXXXX")"
trap 'rm -rf "$tmp" "$TMPDIR/nv-sala-corpo.txt" "$TMPDIR/nv-sala-cab.txt"' EXIT
flags=(-O1 -g -Isrc -Wall -Wextra -Wno-unused-parameter)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/celular.c src/sala.c tests/sala_celular.c -o "$tmp/sala_celular" -lpthread
"$tmp/sala_celular"
