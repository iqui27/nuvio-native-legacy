#!/bin/bash
# Issue #512: addon season/episode for next episode + Trakt scrobble ids.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-trakt512.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Wall -Wextra -ffunction-sections -fdata-sections)
case "$(uname -s)" in
  Darwin) flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wl,-dead_strip) ;;
  *) flags+=(-Wl,--gc-sections) ;;
esac
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
  if [ "$(uname -s)" = Darwin ]; then export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"; fi
fi
"${CC:-cc}" "${flags[@]}" src/trakt.c src/metaep.c tests/stub_fichameta.c src/js.c src/metaprov.c src/traktscrobble.c tests/trakt_season_meta.c \
  -Isrc -o "$dir/teste" -lpthread
"$dir/teste" | grep -E 'trakt_season_meta|PASS'
