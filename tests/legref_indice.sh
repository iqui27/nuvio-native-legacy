#!/bin/bash
# Modo so indice do legref (.tpk 6+) e a engine com fim estimado. Sem fixtures.
#   bash tests/legref_indice.sh     SANITIZE=1 (ASan/UBSan)
set -euo pipefail
cd "$(dirname "$0")/.."
D=$(mktemp -d "${TMPDIR:-/tmp}/nv-legref-idx.XXXXXX"); trap 'rm -rf "$D"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc "${flags[@]}" -Isrc -O1 -g -Wall -Wextra src/legref.c src/autosync.c src/legenda.c src/assrender.c \
  tests/legref_indice.c -pthread -lm -o "$D/t"
"$D/t"
# O mesmo coletor compilado como no .tpk 6+ e no 4/5 liga o modo sozinho.
cc -Isrc -DNV_TPK -fsyntax-only -Wall -Wextra -Werror src/legref.c
cc -Isrc -DNV_TPK -DNV_TPK40 -fsyntax-only -Wall -Wextra -Werror src/legref.c
echo "legref_indice ok"
