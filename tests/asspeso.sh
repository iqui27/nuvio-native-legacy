#!/bin/bash
# Regra pura do peso da legenda ASS (ajuste "Legenda ASS com estilo").
#   bash tests/asspeso.sh
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-asspeso.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
cc -O1 -g -Wall -Wextra -Werror tests/asspeso.c src/asspeso.c -o "$DIR/test"
"$DIR/test"
