#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
bin="$(mktemp "${TMPDIR:-/tmp}/nuvio-homefil.XXXXXXXX")"
trap 'rm -f "$bin"' EXIT
# homefil.c chama fil_*/col_* que o teste dubla; so ele entra.
cc -O1 -g -Isrc -Wno-deprecated-declarations src/homefil.c tests/homefil.c -o "$bin"
"$bin"
