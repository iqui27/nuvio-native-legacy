#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# A regra da Sessao (a sala decide pelo celular). Ver tests/sala.c.
#   bash tests/sala.sh
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "$TMPDIR/nuvio-sala-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
flags=(-O1 -g -Isrc -Wall -Wextra -Werror)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/sala.c tests/sala.c -o "$tmp/sala"
"$tmp/sala"
