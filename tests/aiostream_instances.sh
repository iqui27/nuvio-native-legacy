#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-aio-instances.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -pthread -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -ffunction-sections -fdata-sections)
if [[ $(uname -s) == Darwin ]]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc "${flags[@]}" tests/aiostream_instances_ui.c src/fontesmortas.c src/stream_parse.c src/fontefiltro.c src/linguas.c src/js.c src/badges.c src/streamfit.c src/naovideo.c -o "$work/ui"
"$work/ui"
cc "${flags[@]}" tests/aiostream_instances.c src/js.c src/jsw.c src/watchstate.c src/addonstats.c src/fontefalha.c src/stream_parse.c src/linguas.c src/badges.c src/fontefiltro.c src/idcanal.c src/descoberta.c -o "$work/transport"
"$work/transport"
