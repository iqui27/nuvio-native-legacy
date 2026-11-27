#!/bin/bash
# Embedded Arabic SRT in batches keeps the libass RTL document current.
# See tests/legenda_arabe_lotes.c. Needs fribidi (plainass.c).
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-arabe-lotes.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
FRIBIDI=$(pkg-config --cflags --libs fribidi 2>/dev/null || echo -lfribidi)
cc -DNV_ASS_LIBASS -Isrc -O1 -g -Wall -Wextra tests/legenda_arabe_lotes.c src/legenda.c src/plainass.c src/asssinais.c \
  -I/opt/homebrew/include $FRIBIDI -lm -lpthread -o "$DIR/test"
"$DIR/test"
