#!/bin/bash
# Cache VOD de metadados, com addons.c real e HTTP/video/debrid dublados.
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fontecache-vod.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Isrc -pthread -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wall -Wextra -ffunction-sections -fdata-sections -Wl,-dead_strip)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/fontecache_vod.c src/addons.c src/addonstats.c src/js.c -o "$dir/teste"
"$dir/teste"
