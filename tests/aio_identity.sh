#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-aio-identity.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -O1 -g -pthread -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -ffunction-sections -fdata-sections -Wl,-dead_strip tests/aiostream_instances.c src/js.c src/jsw.c src/watchstate.c src/addonstats.c src/fontefalha.c src/stream_parse.c src/linguas.c src/badges.c src/fontefiltro.c src/idcanal.c src/descoberta.c src/addonmodo.c -o "$work/transport"
"$work/transport"
