#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-epg-streaming.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=c11 -D_POSIX_C_SOURCE=200809L -O1 -g -Wall -Wextra -Werror -Isrc)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/epg_streaming.c src/xmltv.c -lz -lpthread -o "$work/test"
"$work/test" "$work/data"
