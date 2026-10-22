#!/bin/bash
# #244 (cinco cards iguais, remocao parcial) e #243 ("14" inventado, nota ausente).
#   bash tests/trakt_cw_dup.sh
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-traktcwdup.XXXXXX")
trap 'rm -rf "$dir"' EXIT
cc src/trakt.c tests/stub_fichameta.c src/js.c src/metaprov.c tests/trakt_cw_dup.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$dir/teste" -O1 -g -Wall -Wextra -Wl,-dead_strip -lpthread
"$dir/teste" >/dev/null
COM_CERT=1 "$dir/teste" >/dev/null
SEM_NOTA=1 "$dir/teste" >/dev/null
"$dir/teste" | tail -1
