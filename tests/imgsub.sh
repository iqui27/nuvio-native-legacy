#!/bin/bash
# Armazem de quadros de legenda de imagem (PGS) por tempo.
#   bash tests/imgsub.sh
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-imgsub.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc ${flags[@]+"${flags[@]}"} -Isrc tests/imgsub.c src/imgsub.c -o "$DIR/test" -O1 -g -Wall -Wextra -lpthread
"$DIR/test"
