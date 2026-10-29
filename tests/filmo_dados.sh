#!/bin/bash
set -eu
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
mkdir -p "$TMPDIR"
work=$(mktemp -d "$TMPDIR/filmo-dados.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work"
cc tests/filmo_dados.c src/pessoa.c src/js.c src/filmomodelo.c src/conquistasregra.c src/fpscena.c -Isrc -o "$work/test" -O0 -I/opt/homebrew/include -Wl,-dead_strip -Wl,-undefined,dynamic_lookup
if [ "${1:-episodes}" = performance ]; then
  for scenario in performance episodes publication provenance scheduling quiz; do
    "$work/test" "$scenario"
  done
else
  "$work/test" "${1:-episodes}"
fi
