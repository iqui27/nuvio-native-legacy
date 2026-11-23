#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
grep -q 'arabico: CP1256' src/legenda.c
grep -q '!defined(NV_TPK40)' src/legref.c || grep -q '#if defined(NV_TPK)' src/legref.c
test -f tests/legref_indice.sh
echo "PASS: arabe CP1256 + legref TPK40 (#751)"
