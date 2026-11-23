#!/bin/bash
# #691: contrato minimo — log de plugins expoe [plugins] ao falhar (sem aparelho).
set -euo pipefail
cd "$(dirname "$0")/.."
grep -q '\[plugins\]' src/plugins.c
grep -q 'sem codigo\|desistiu\|prazoMs' src/plugins.c
grep -q '#691: consulta descartada' src/plugins.c
echo "PASS: plugins.c registra falhas de scraper (diagnostico #691)"
