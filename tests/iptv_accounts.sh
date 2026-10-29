#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-iptv-accounts.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -D_DARWIN_C_SOURCE -O1 -g -Wall -Wextra -Isrc)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/iptv_accounts.c src/iptvfontes.c src/xtream.c src/m3u.c src/m3u_parse.c src/stalker.c src/js.c -lpthread -o "$work/test"
"$work/test" "$work/data"
