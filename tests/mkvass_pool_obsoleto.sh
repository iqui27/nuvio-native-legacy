#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# #330: pool do mkvass nao baixa Range de geracao velha. Sem rede, sem midia.
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-mkvass-pool.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
flags=()
if [ "$(uname -s)" != Darwin ]; then flags+=(-pthread -lm); fi
cc tests/mkvass_pool_obsoleto.c src/legenda.c src/imgsub.c src/pgs.c src/assrender.c src/dados.c -Isrc -O1 -g -Wno-deprecated-declarations \
  ${flags[@]+"${flags[@]}"} -lz -o "$DIR/test"
"$DIR/test"
