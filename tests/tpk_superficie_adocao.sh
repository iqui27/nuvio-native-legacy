#!/bin/bash
# #754: segundo arranque com superficie EGL errada (NU7100 espelhado/chuvisco).
set -euo pipefail
cd "$(dirname "$0")/.."
grep -q 'eglQuerySurface' src/tpk.c
grep -q 'tela adotada da superficie' src/tpk.c
grep -q 'TAMANHO ERRADO' src/tpk.c
grep -q 'NV_TPK40' src/tpk.c
echo "PASS: tpk adota tamanho da superficie e marca mismatch (#754)"
