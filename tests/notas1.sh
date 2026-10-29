#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# mdblist ratings[] (#205): GET /imdb/movie/tt... devolve source/value/score; o parser
# antigo (POST + "rating") deixava 0/N no aparelho.
set -eu
cd "$(dirname "$0")/.."
sources=(src/extras.c src/notasfontes.c)
# Optional revision reproduces the display assertion against unmodified sources.
if [ "$#" -gt 0 ]; then
  baseline=$(mktemp -d "$TMPDIR/notas1-baseline.XXXXXX")
  trap 'rm -rf "$baseline"' EXIT
  git show "$1:src/extras.c" > "$baseline/extras.c"
  git show "$1:src/notasfontes.c" > "$baseline/notasfontes.c"
  sources=("$baseline/extras.c" "$baseline/notasfontes.c")
fi
cc -O1 -g -Wall -Wextra -DNV_EXTRAS_TESTE -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  "${sources[@]}" src/js.c tests/notas1.c \
  -lpthread -o ${TMPDIR:-/tmp}/nuvio-notas1-tests
${TMPDIR:-/tmp}/nuvio-notas1-tests
