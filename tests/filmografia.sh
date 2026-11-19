#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
mkdir -p "$TMPDIR"
bin=$(mktemp "$TMPDIR/filmografia.XXXXXX")
trap 'rm -f "$bin"' EXIT
cc -std=c99 -Wall -Wextra -Werror -Isrc tests/filmografia.c src/filmomodelo.c src/conquistasregra.c -o "$bin"
"$bin"
