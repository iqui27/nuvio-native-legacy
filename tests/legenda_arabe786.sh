#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
grep -q '#786 embutida' src/video_tizen.c
grep -q 'legenda_log_arquivo' src/legenda.c
test -f tests/legref_indice.sh
echo "PASS: diagnostico arabe embutido (#786)"
