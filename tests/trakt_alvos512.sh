#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-trakt-alvos512.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Wall -Wextra -ffunction-sections -fdata-sections -Isrc
  -DNV_DUBLE_SESSAO -Dpthread_detach=fixture_detach)
case "$(uname -s)" in
  Darwin) flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wl,-dead_strip) ;;
  *) flags+=(-Wl,--gc-sections) ;;
esac
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/trakt.c src/js.c src/jsw.c src/metaep.c src/traktscrobble.c \
  tests/trakt_alvos512.c -lpthread -o "$dir/teste"
"$dir/teste"
