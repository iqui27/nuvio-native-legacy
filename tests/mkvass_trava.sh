#!/bin/bash
# imgsub_limpar fora de S.trava (sem inversao de trava com o desenho).
#   bash tests/mkvass_trava.sh
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkvass-trava.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
cc -Isrc -O1 -g -Wall -Wextra tests/mkvass_trava.c src/mkvass.c src/pgs.c src/legenda.c tests/mkvass_i18n.c src/assrender.c src/dados.c -o "$DIR/test" 2>/dev/null
mkdir "$DIR/dados"
NUVIO_DADOS="$DIR/dados" "$DIR/test"
