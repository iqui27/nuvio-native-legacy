#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Decisao e corpo do scrobble do Trakt (#179). Sem SDL e sem rede.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-trakt-scrobble.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Wall -Wextra)
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
  if [ "$(uname -s)" = Darwin ]; then export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"; fi
fi
"${CC:-cc}" "${flags[@]}" src/traktscrobble.c src/metaep.c src/js.c tests/trakt_scrobble.c -Isrc -o "$dir/teste"
"$dir/teste"
