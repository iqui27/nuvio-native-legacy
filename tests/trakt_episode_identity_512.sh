#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-episode-id-512.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Wall -Wextra -ffunction-sections -fdata-sections -Isrc)
case "$(uname -s)" in
  Darwin) flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wl,-dead_strip) ;;
  *) flags+=(-Wl,--gc-sections) ;;
esac
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/trakt.c tests/stub_fichameta.c src/js.c src/metaep.c src/traktscrobble.c src/metaprov.c \
  src/proximo.c tests/trakt_episode_identity_512.c -lpthread -o "$dir/teste"
for modo in normal missing duplicate offline mapped; do "$dir/teste" "$modo"; done
