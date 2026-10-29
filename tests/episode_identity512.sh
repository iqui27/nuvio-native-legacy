#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" \
  tests/episode_identity512.c src/metaep.c src/traktscrobble.c src/js.c \
  -Isrc -O1 -g -Wall -Wextra -o "${TMPDIR:-/tmp}/nuvio-episode-identity512"
"${TMPDIR:-/tmp}/nuvio-episode-identity512"
