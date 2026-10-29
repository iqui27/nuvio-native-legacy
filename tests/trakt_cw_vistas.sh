#!/bin/bash
# Issue traktcw: "a seguir" de series so do Trakt + selo de serie vista no arranque.
#   bash tests/trakt_cw_vistas.sh
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-traktcw.XXXXXX")
trap 'rm -rf "$dir"' EXIT
cc src/trakt.c src/metaep.c tests/stub_fichameta.c src/js.c src/metaprov.c tests/trakt_cw_vistas.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$dir/teste" -O1 -g -Wall -Wextra -Wl,-dead_strip -lpthread
for mode in current missing stale; do
  TRAKT_META="$mode" "$dir/teste" | grep -E 'trakt-cw-vistas|PASS'
done
