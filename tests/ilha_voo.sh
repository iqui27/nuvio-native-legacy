#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
bin=$(mktemp "${TMPDIR:-/tmp}/nuvio-ilha-voo.XXXXXX")
trap 'rm -f "$bin"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/ilha_voo.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$bin" -O1 -g ${flags[@]+"${flags[@]}"} -Wno-deprecated-declarations
"$bin"
