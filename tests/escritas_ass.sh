#!/bin/bash
# Hebrew/Arabic run marking for authored ASS. See tests/escritas_ass.c.
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-escritas-ass.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
cc -Isrc -O1 -g -Wall -Wextra tests/escritas_ass.c src/assrender.c src/asssinais.c -o "$DIR/test" -lm -lpthread
"$DIR/test"
