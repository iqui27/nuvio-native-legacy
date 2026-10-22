#!/bin/bash
# Aquecer conexoes (2.0.2): o planejamento sem rede. Ver tests/aquecer.c.
#
#   bash tests/aquecer.sh
set -eu
cd "$(dirname "$0")/.."
bin="${TMPDIR:-/tmp}/nuvio-aquecer-tests"
cc -O1 -g -Wall -Wextra -DAQUECER_TESTE -Isrc src/aquecer.c tests/aquecer.c -o "$bin" -lpthread
"$bin"

# CONTRATO: aquecer nunca pede caminho de stream. O motor (rede.c) so faz HEAD
# em "<origem>/", sem seguir redirecionamento.
corpo=$(awk '/^int rede_aquecer_lote\(const char \*const \*origens, int n, unsigned \*ms\) \{$/,/^}/' src/rede.c | head -80)
if [ -z "$corpo" ]; then echo "aquecer: rede_aquecer_lote sumiu de rede.c"; exit 1; fi
if ! printf '%s' "$corpo" | grep -q 'OPT_NOBODY' || ! printf '%s' "$corpo" | grep -q 'OPT_FOLLOWLOCATION, (long)0'; then
  echo "aquecer: o motor deixou de ser HEAD sem redirecionamento"; exit 1
fi
echo "aquecer: contrato de rede.c ok"
