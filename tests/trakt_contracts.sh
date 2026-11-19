#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-traktcontracts.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Wall -Wextra -ffunction-sections -fdata-sections)
case "$(uname -s)" in
  Darwin) flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wl,-dead_strip) ;;
  *) flags+=(-Wl,--gc-sections) ;;
esac
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
"${CC:-cc}" "${flags[@]}" -DNV_DUBLE_SESSAO src/trakt.c src/traktscrobble.c \
  src/metaep.c src/js.c src/jsw.c tests/stub_fichameta.c tests/trakt_contracts.c \
  -Isrc -o "$dir/teste" -lpthread
"$dir/teste"
