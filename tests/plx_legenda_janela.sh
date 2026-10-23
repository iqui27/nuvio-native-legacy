#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
mkdir -p "$TMPDIR"
dir=$(mktemp -d "$TMPDIR/nuvio-plx-janela.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=()
if [ "$(uname -s)" != Darwin ]; then flags+=(-pthread -lm); fi
cc tests/plx_legenda_janela.c src/legenda.c src/assrender.c src/dados.c src/imgsub.c src/pgs.c \
  -Isrc -O1 -g -Wno-deprecated-declarations "${flags[@]}" -o "$dir/test"
"$dir/test"
