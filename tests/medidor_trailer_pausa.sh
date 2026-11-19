#!/bin/bash
# Medidor volta na pausa do trailer nativo em tela cheia (rev. do #338, job 205).
# Ver o .c.
#
#   bash tests/medidor_trailer_pausa.sh
set -eu
cd "$(dirname "$0")/.."
TR_DIR=$(mktemp -d "${TMPDIR:-/tmp}"/nuvio-medidor-trailer-pausa.XXXXXX)
trap 'rm -rf "$TR_DIR"' EXIT
cc -DNV_TPK -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -ffunction-sections -fdata-sections tests/medidor_trailer_pausa.c src/trailerfonte.c \
  -Wl,-dead_strip,-undefined,dynamic_lookup -o "$TR_DIR/test"
"$TR_DIR/test"