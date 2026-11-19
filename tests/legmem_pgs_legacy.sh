#!/bin/bash
# Legacy saved PGS numero -1 must still resolve after recovered tracks got -2, -3, …
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-legmem-pgs.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/legmem_pgs_legacy.c -Isrc -Itests \
  -std=gnu11 -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -Wall -Wextra -Wno-unused-function -o "$work/t"
"$work/t"
