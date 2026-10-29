#!/bin/bash
set -eu
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
mkdir -p "$TMPDIR"
bin=$(mktemp "$TMPDIR/pessoa.XXXXXX")
trap 'rm -f "$bin"' EXIT
cc -std=c99 -Wall -Wextra -Isrc tests/filmo_pessoa.c src/js.c src/filmomodelo.c src/conquistasregra.c -pthread -o "$bin"
"$bin" "${1:-date}"
