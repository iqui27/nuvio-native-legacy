#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-warm.XXXXXX")
trap 'rm -rf "$dir"' EXIT
cc -O1 -g -Wall -Isrc -DAQUECER_TESTE src/retomamorna.c src/aquecer.c tests/retomamorna.c -lpthread -o "$dir/test"
"$dir/test"
