#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-server-library.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/biblioteca_servidores.c src/salvos.c src/focus.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -g \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -o "$work/test"
"$work/test"
cc tests/servidores_setup.c src/srvsetup_net.c src/jellyfin.c src/js.c src/redeurl.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -o "$work/setup"
"$work/setup"
cc tests/servidores_setup_http.c src/srvsetup_net.c src/jellyfin.c src/js.c src/jsw.c src/redeurl.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -o "$work/http"
"$work/http"
