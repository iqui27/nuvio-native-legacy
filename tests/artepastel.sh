#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
bin=$(mktemp "${TMPDIR:-/tmp}/nuvio-artepastel.XXXXXX")
trap 'rm -f "$bin"' EXIT
cc -Wall -Wextra -O2 tests/artepastel.c src/corviva.c -Isrc -o "$bin" -lm
"$bin"
