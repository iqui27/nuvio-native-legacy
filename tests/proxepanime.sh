#!/bin/bash
# Issue #697: Next Episode on an anime opened an unrelated title.
#   bash tests/proxepanime.sh
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-proxepanime.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=gnu11 -O1 -g -pthread -Isrc -ffunction-sections -fdata-sections
       -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-macro-redefined)
if [[ $(uname -s) == Darwin ]]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc "${flags[@]}" tests/proxepanime.c src/catalogo.c -o "$work/test"
"$work/test"
