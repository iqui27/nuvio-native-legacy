#!/bin/bash
# srvidx: personal-server matching index (rules, titles, disk, incremental).
#   bash tests/srvidx.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-srvidx.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
flags=(-O1 -g -std=gnu11 -Wall -Wextra -Werror -Isrc)
if [ "${NV_SANITIZERS:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/srvidx.c src/srvidx.c -o "$tmp/teste"
"$tmp/teste"
