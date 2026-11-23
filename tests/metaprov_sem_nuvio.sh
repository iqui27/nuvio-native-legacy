#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-metaprov794.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -O1 -Isrc tests/metaprov_sem_nuvio.c src/metaprov.c src/js.c -o "$work/test" -lpthread
"$work/test"
