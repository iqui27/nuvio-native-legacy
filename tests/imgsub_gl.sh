#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d /tmp/nuvio-imgsub-gl.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc ${flags[@]+"${flags[@]}"} -Isrc -O1 -g -Wall -Wextra tests/imgsub_gl.c src/imgsub.c src/imgsub_gl.c -lpthread -o "$DIR/test"
"$DIR/test"
