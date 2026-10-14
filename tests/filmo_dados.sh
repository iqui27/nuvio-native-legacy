#!/bin/bash
set -eu
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
mkdir -p "$TMPDIR"
work=$(mktemp -d "$TMPDIR/filmo-dados.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work"
cc tests/filmo_dados.c src/pessoa.c src/js.c src/filmomodelo.c src/conquistasregra.c -Isrc -o "$work/test" -O0 -I/opt/homebrew/include -Wl,-dead_strip -Wl,-undefined,dynamic_lookup
"$work/test"
