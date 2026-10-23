#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Press-dip do OK: o card afunda enquanto a tecla esta apertada. Ver tests/pressdip.c.
set -eu
cd "$(dirname "$0")/.."
cc -Wall -Wextra -O2 tests/pressdip.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-pressdip-test -lm
${TMPDIR:-/tmp}/nuvio-pressdip-test
rm -f ${TMPDIR:-/tmp}/nuvio-pressdip-test
grep -q 'pressdip_passo' src/home.c || { echo "FALHA: home.c nao usa pressdip_passo"; exit 1; }
# Palco: palcoGeo sobrescreve esc; o okDip tem de ser aplicado depois dele.
grep -q 'palcoGeo(r, c, lw, &cx, &esc);[^;]*okDip' src/home.c || { echo "FALHA: Palco descarta okDip"; exit 1; }
