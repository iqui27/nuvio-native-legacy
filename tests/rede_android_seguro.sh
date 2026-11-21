#!/bin/bash
# Reserva segura do download verificado com a libcurl fora (F94).
# Ver tests/rede_android_seguro.c.
#   bash tests/rede_android_seguro.sh [--sanitize]
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-android-seguro.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
flags=(-O1 -g -std=gnu11 -Wall -Wextra -Werror -Wno-unused-function -Isrc -pthread)
if [ "$(uname -s)" != Darwin ]; then flags+=(-ldl); fi
if [ "${1:-}" = --sanitize ] || [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
cc "${flags[@]}" tests/rede_android_seguro.c src/redeurl.c -o "$DIR/teste"
"$DIR/teste"
