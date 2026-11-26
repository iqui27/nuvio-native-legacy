#!/bin/bash
# TorBox file that never shows a picture: recovery worker + early lookup (stubs, no network).
set -euo pipefail
cd "$(dirname "$0")/.."
D=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tbabrir.XXXXXX")
trap 'rm -rf "$D"' EXIT
if [[ $(uname -s) == Linux ]]; then read -ra CF <<< "$(pkg-config --cflags sdl2)"; else CF=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2); fi
cc -std=gnu11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -DTBA_REGISTRO_MS=20 -Isrc "${CF[@]}" \
  src/tbabrir.c tests/tbabrir.c -o "$D/t" -lpthread
"$D/t"
