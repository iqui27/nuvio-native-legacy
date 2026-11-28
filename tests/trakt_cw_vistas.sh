#!/bin/bash
# Issue traktcw: "a seguir" de series so do Trakt + selo de serie vista no arranque.
#   bash tests/trakt_cw_vistas.sh
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-traktcw.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Wall -Wextra -ffunction-sections -fdata-sections -Isrc)
case "$(uname -s)" in
  Darwin) flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wl,-dead_strip);;
  *) flags+=(-Wl,--gc-sections);;
esac
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/trakt.c src/metaep.c tests/stub_fichameta.c src/js.c src/metaprov.c tests/trakt_cw_vistas.c \
  -o "$dir/teste" -lpthread
for mode in current missing stale; do
  TRAKT_META="$mode" "$dir/teste" | grep -E 'trakt-cw-vistas|PASS'
done
