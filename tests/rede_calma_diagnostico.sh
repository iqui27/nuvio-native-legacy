#!/bin/bash
# #728: diagnostico offline — pausa de host por conexao recusada (calmaHost).
set -euo pipefail
cd "$(dirname "$0")/.."
grep -q 'calmaFalta' src/rede.c
grep -q 'em pausa (recusou conexao' src/rede.c
grep -q 'recusou conexao (curl' src/rede.c
grep -q 'NV_TPK40' tools/tpk.sh
echo "PASS: logs de calmaHost presentes para correlacionar queda TPK40 (#728)"
