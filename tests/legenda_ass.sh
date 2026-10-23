#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Parser ASS/SSA (#92). So legenda.c + adaptador ASS + stub de rede: sem SDL,
# sem rede.
#   bash tests/legenda_ass.sh
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-legenda-ass.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc ${flags[@]+"${flags[@]}"} src/assrender.c src/legenda.c tests/legenda_ass.c -Isrc -o "$DIR/test" -O1 -g \
  -Wall -Wno-deprecated-declarations
"$DIR/test"
