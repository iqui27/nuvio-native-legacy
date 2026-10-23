#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# WebP animado pelo caminho do GIF (#141), com a libwebp do Homebrew.
#
#   bash tests/gifwebp.sh
#
# O mesmo src/gif.c das TVs, com NV_WEBP_ANIM: no Tizen quem liga a flag e
# tools/tizen.sh, com a libwebp de tools/build-webp-wasm.sh.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} -DNV_WEBP_ANIM src/gif.c tests/gifwebp.c \
  -Isrc -I/opt/homebrew/include -L/opt/homebrew/lib -lwebpmux -lwebpdemux -lwebp \
  -o ${TMPDIR:-/tmp}/nuvio-gifwebp-tests -O1 -g -pthread \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-gifwebp-tests
