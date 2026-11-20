#!/bin/bash
# Sincronia de legenda por audio (F06): VAD/alinhamento com PCM sintetico, anel,
# sessao inteira ate a engine, cancelamentos, passthrough, plataforma.
#   bash tests/audsync.sh    SANITIZE=1 (ASan/UBSan) ou SANITIZE=thread
set -euo pipefail
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}
D=$(mktemp -d "$T/nv-audsync.XXXXXX"); trap 'rm -rf "$D"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
elif [ "${SANITIZE:-0}" = thread ]; then flags+=(-fsanitize=thread -fno-omit-frame-pointer); fi
cc -Isrc -Wall -Wextra -Werror -fsyntax-only src/audvad.c src/audsync.c
cc "${flags[@]}" -Isrc -Itests -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -g -Wall -Wextra \
  src/audvad.c src/audsync.c src/legsync.c src/legsyncui.c src/legenda2.c src/legref.c src/autosync.c src/legenda.c src/assrender.c \
  tests/audsync.c -pthread -lm -o "$D/t"
"$D/t" 2>&1 | grep -v '^\[legenda\]'
