#!/bin/bash
# A corrida da adocao: escolha por ordinal antes da publicacao do ordinal
# preserva o pacing lateral (janela de 12 s). Sem rede, sem midia.
set -eu
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
mkdir -p "$TMPDIR"
dir=$(mktemp -d "$TMPDIR/nuvio-plx-adocao.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=()
if [ "$(uname -s)" != Darwin ]; then flags+=(-pthread -lm); fi
cc tests/plx_legenda_adocao.c src/legenda.c src/assrender.c src/dados.c src/pgs.c src/imgsub.c \
  -Isrc -O1 -g -Wno-deprecated-declarations "${flags[@]}" -o "$dir/test"
"$dir/test"