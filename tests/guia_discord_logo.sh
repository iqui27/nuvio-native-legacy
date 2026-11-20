#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/tmp}"
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} tests/guia_discord_logo.c -Isrc \
  -o "${TMPDIR%/}/nuvio-guia-discord-logo" -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -lpthread
"${TMPDIR%/}/nuvio-guia-discord-logo"
