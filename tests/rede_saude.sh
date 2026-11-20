#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rede-saude.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
cc -O1 -g -std=gnu11 -Wall -Wextra -Werror -Isrc tests/rede_saude.c src/redeurl.c -pthread -o "$tmp/teste"
# retry inclui a rajada: 3 GETs paralelos, 5 s + 5 s, offline aos 10 s.
"$tmp/teste" "${1:-retry}"
