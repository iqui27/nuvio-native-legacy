#!/bin/bash
# Decodificador PGS (S_HDMV/PGS) em C puro: fixture gerado no proprio teste.
#   bash tests/pgs.sh
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-pgs.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc ${flags[@]+"${flags[@]}"} -Isrc tests/pgs.c src/pgs.c -o "$DIR/test" -O1 -g -Wall -Wextra
"$DIR/test"
