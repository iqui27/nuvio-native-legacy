#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# OpenSeek: VTT (origem, t=, chave), regra [ini, fim) e Seekr inalterado.
#
#   bash tests/openseek_vtt.sh
#
# So C puro: seekrvtt.c, openseekproto.c, js.c, redeurl.c — sem rede nem SDL.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/seekrvtt.c src/openseekproto.c src/osp1.c src/js.c src/redeurl.c tests/openseek_vtt.c \
  -Isrc -o "$TMPDIR/nuvio-openseek-vtt" -O1 -g \
  -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations
"$TMPDIR/nuvio-openseek-vtt"
