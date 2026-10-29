#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guia-grupos.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
backup_extra=()
if [ -f src/backupcategories.c ]; then backup_extra+=(src/backupcategories.c); fi
cc ${backup_extra[@]+"${backup_extra[@]}"} tests/guia_grupos.c src/guiaaddons.c src/guiafiltro.c src/js.c src/ltv.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -g "${flags[@]}" -lpthread -lm -o "$work/test"
"$work/test" "$work"
