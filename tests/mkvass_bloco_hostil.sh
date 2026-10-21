#!/bin/bash
# Block declarado enorme (hostil). Ver tests/mkvass_bloco_hostil.c.
#   bash tests/mkvass_bloco_hostil.sh
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkvass-tracks-grande.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
FONTES="tests/mkvass_bloco_hostil.c src/mkvass.c src/imgsub.c src/pgs.c src/legenda.c src/assrender.c src/dados.c"
cc -Isrc -O1 -g -Wall -Wextra -Wno-deprecated-declarations $FONTES -o "$DIR/test"
mkdir "$DIR/dados"
(cd "$DIR" && NUVIO_DADOS="$DIR/dados" ./test)
