#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-aio-catalogs.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -O1 -g -Isrc tests/aio_catalogs.c src/aiostreams.c src/js.c -o "$work/test"
"$work/test"
