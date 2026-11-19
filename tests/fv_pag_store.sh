#!/bin/bash
# Issue #789 Phase-1: VtStore per-item cost vs the 8 MiB budget.
#   bash tests/fv_pag_store.sh
set -euo pipefail
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}"
cc -O1 -Isrc tests/fv_pag_store.c -o "$out/fv-pag-store"
"$out/fv-pag-store"
