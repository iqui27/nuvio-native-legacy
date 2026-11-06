#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# OpenSeek: montagem de pedidos (pino, URL so no corpo), JSON, respostas,
# estados SEEKR_*, endereco e LRU por chave. C puro, sem rede nem SDL.
#
#   bash tests/openseek_pedido.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/openseekproto.c src/osp1.c src/seekrvtt.c src/js.c src/redeurl.c tests/openseek_pedido.c \
  -Isrc -o "$TMPDIR/nuvio-openseek-pedido" -O1 -g \
  -Wall -Wextra -Wno-unused-parameter
"$TMPDIR/nuvio-openseek-pedido"
