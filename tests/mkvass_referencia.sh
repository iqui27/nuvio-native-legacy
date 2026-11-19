#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
D=$(mktemp -d "${TMPDIR:-/tmp}/nv-mkvass-ref.XXXXXX"); trap 'rm -rf "$D"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc -Isrc -O1 -g -Wall -Wextra -Wno-deprecated-declarations tests/mkvass_referencia.c \
  src/legref.c src/autosync.c src/legenda.c src/assrender.c -pthread -lm "${flags[@]}" -o "$D/t"
"$D/t"
