#!/bin/bash
# #308: faixa ASS cujo Tracks passa de 64 KB. Ver tests/mkvass_tracks_grande.c.
#   bash tests/mkvass_tracks_grande.sh
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkvass-tracks-grande.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
FONTES="tests/mkvass_tracks_grande.c src/mkvass.c src/legenda.c src/assrender.c src/dados.c"
cc -Isrc -O1 -g -Wall -Wextra -Wno-deprecated-declarations $FONTES -o "$DIR/test"
mkdir "$DIR/dados"
(cd "$DIR" && NUVIO_DADOS="$DIR/dados" ./test)
