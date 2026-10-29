#!/bin/bash
# Reuse explicitly supplied capture binaries, or build the current tree.
set -euo pipefail
cd "$(dirname "$0")/.."
export NV_ONLY_NEW=1 NV_KEEP=1
export NV_PNG_OUT=/Volumes/ExternalSSD/tmp/textoescala/fix2/after
export NV_PNG_PICK='salvos-cartaz|1-serie-chamada|tela-inicial|painel|L1-g0-1-fileira|campo|abas-grade|normal'
mkdir -p "$NV_PNG_OUT"
bash tests/textoescala_prova.sh
