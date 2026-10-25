#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
mkdir -p "$TMPDIR"
NUVIO_DADOS=$(mktemp -d "$TMPDIR/filmo-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
BIN="$TMPDIR/filmo-shot"
bash tests/j4_compile.sh tests/filmografia_shot.c "$BIN"
"$BIN" "${1:-/Volumes/ExternalSSD/tmp/rev/filmo-impl}"
