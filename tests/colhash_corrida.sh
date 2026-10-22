#!/bin/bash
# Hash das colecoes no fio da montagem x sync (#203). Ver tests/colhash_corrida.c.
#   bash tests/colhash_corrida.sh                  (ASAN+UBSAN, o padrao)
#   SANITIZE=thread bash tests/colhash_corrida.sh  (TSAN)
#   SANITIZE=0 bash tests/colhash_corrida.sh       (so os asserts)
# Evidencia de antes (hash do jeito de 0fcfa601): CFLAGS_X=-DANTIGO
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -pthread -Wall -Wno-misleading-indentation)
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread) ;;
esac
[ -n "${CFLAGS_X:-}" ] && flags+=(${CFLAGS_X})
bin="${TMPDIR:-/tmp}/nuvio-colhash-corrida"
cc "${flags[@]}" src/colecoes.c src/redeurl.c src/js.c tests/colhash_corrida.c -o "$bin"
"$bin"
