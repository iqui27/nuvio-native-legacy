#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rede-dns.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
cc -O1 -g -std=gnu11 -Wall -Wextra -Werror -DNV_TPK -Isrc tests/rede_dns.c src/redeurl.c -pthread -o "$tmp/test"
"$tmp/test"
