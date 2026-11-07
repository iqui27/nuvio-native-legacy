#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# OpenSeek, fonte alternativa para miniaturas: corpos JSON e bloco "alt". C puro.
#
#   bash tests/openseek_alt.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/openseekproto.c src/osp1.c src/seekrvtt.c src/js.c src/redeurl.c tests/openseek_alt.c \
  -Isrc -o "$TMPDIR/nuvio-openseek-alt" -O1 -g \
  -Wall -Wextra -Wno-unused-parameter
"$TMPDIR/nuvio-openseek-alt"
