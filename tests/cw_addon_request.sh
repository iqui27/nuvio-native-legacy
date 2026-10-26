#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-cw-addon.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=gnu11 -O1 -g -pthread -Isrc -ffunction-sections -fdata-sections
       -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-macro-redefined)
if [[ $(uname -s) == Darwin ]]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc "${flags[@]}" tests/cw_addon_request.c src/idcanal.c src/js.c src/jsw.c src/watchstate.c src/addonstats.c src/fontefalha.c -o "$work/test"
"$work/test"
cc "${flags[@]}" tests/cw_addon_identity.c src/catalogo.c -o "$work/identity"
"$work/identity"
