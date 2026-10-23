#!/bin/bash
# Plugin ordering, round accounting and shared empty-source message; no network.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-plugins-fila.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-DNV_WEBOS -DPLUG_TETO_MS=150 -std=gnu11 -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -pthread -ffunction-sections -fdata-sections -Wno-macro-redefined)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc "${flags[@]}" tests/plugins_fila.c src/js.c src/rede.c -o "$work/t"
"$work/t"
python3 tools/idiomas.py
