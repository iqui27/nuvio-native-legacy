#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# OpenSeek SR-3: CURLOPT_PINNEDPUBLICKEY aplicado pelo caminho que falha
# fechado, contra o rede_pedido.inc real e uma libcurl falsa.
#
#   bash tests/openseek_rede.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/redeurl.c tests/openseek_rede.c \
  -Isrc -o "$TMPDIR/nuvio-openseek-rede" -O1 -g \
  -Wall -Wno-unused-parameter -Wno-unused-function -Wno-misleading-indentation
"$TMPDIR/nuvio-openseek-rede"
