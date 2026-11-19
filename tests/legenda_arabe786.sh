#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
grep -q 'arabico: CP1256' src/legenda.c
echo "PASS: diagnostico arabe embutido (#786)"
