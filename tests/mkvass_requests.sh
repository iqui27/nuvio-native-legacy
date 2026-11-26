#!/bin/bash
# #308: conta Ranges da leitura de legenda ASS (mesmo arquivo, antes/depois). Ver tests/mkvass_requests.c.
#   bash tests/mkvass_requests.sh
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkvass-requests.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
FONTES="tests/mkvass_requests.c src/mkvass.c src/pgs_mkv_restore.c src/imgsub.c src/pgs.c src/legenda.c tests/mkvass_i18n.c src/assrender.c src/assperf.c src/asspeso.c src/assfios.c src/dados.c"
cc -Isrc -O1 -g -Wall -Wextra -Wno-deprecated-declarations $FONTES -lz -lm -o "$DIR/test"
mkdir "$DIR/dados"
(cd "$DIR" && NUVIO_DADOS="$DIR/dados" ./test)
