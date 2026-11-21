#!/bin/bash
# #767: trecho do PR — legref no TPK 4/5.
set -euo pipefail
cd "$(dirname "$0")/.."
grep -q 'NV_TPK' src/legref.c
! grep -q 'NV_TPK40' src/legref.c || grep -q 'pthread_key' src/legref.c
test -f tests/legref_indice.sh
grep -q 'legref_disponivel' src/legref.c
echo "PASS: legref TPK40 do PR #767 (compilar legref_indice no Mac)"
