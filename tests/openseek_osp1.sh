#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# OSP1: SHA-256/HMAC/PBKDF2 e a derivacao do pareamento OpenSeek contra
# tests/openseek_vectors.json (gerado por tests/openseek_vectors.py so do
# contrato). OPENSEEK_VECTORS=<osp1.json do backend> confere aqueles tambem.
#
#   bash tests/openseek_osp1.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/osp1.c src/js.c tests/openseek_osp1.c \
  -Isrc -o "$TMPDIR/nuvio-openseek-osp1" -O2 -g \
  -Wall -Wextra -Wno-unused-parameter
"$TMPDIR/nuvio-openseek-osp1"
