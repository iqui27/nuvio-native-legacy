#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guia-addon.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-ffunction-sections -fdata-sections -Wl,--gc-sections)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/guia_addon_filtro.c src/guiaaddons.c src/guiafiltro.c src/js.c src/ltv.c -Isrc -I/usr/include/SDL2 -O1 -g "${flags[@]}" -lpthread -lm -o "$work/teste"
"$work/teste" "$work"
