#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
san=()
if [ "${SANITIZE:-0}" = 1 ]; then san=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc -O1 -g -Wall -Wextra -pthread "${san[@]}" -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 -Wno-deprecated-declarations \
  src/webp.c tests/webp.c -o ${TMPDIR:-/tmp}/nuvio-webp-tests
${TMPDIR:-/tmp}/nuvio-webp-tests
