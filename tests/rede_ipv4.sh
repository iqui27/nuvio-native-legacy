#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rede-ipv4.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
cc -O1 -g -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Isrc tests/rede_ipv4.c src/redeurl.c -pthread -o "$tmp/teste"
"$tmp/teste"
cc -O1 -g -std=gnu11 -Wall -Wextra -Werror -Isrc tests/rede_ipv4_real.c src/rede.c src/redeurl.c -pthread -ldl -o "$tmp/real"
"$tmp/real" "$tmp/real.log"
