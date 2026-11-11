#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
t=$(mktemp -d); trap 'rm -rf "$t"' EXIT
cc -std=gnu11 -D_GNU_SOURCE -w -Isrc tests/celular_cgnat.c -o "$t/test" -lpthread
"$t/test"
