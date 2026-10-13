#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d /tmp/nuvio-conquistas-conta.XXXXXX)
trap 'rm -rf "$tmp"' EXIT
cc -Wall -Wextra tests/conquistas_conta.c src/conquistasdados.c src/conquistasregra.c -Isrc -o "$tmp/test"
cd "$tmp"
./test
