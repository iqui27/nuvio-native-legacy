#!/bin/bash
# Decimal por idioma (idioma_decimal_texto em idiomacod.h). Sem rede, sem SDL.
#   bash tests/idioma_decimal.sh
set -eu
cd "$(dirname "$0")/.."
bin="${TMPDIR:-/tmp}/nuvio-idioma-decimal"
cc -O1 -Wall -Isrc tests/idioma_decimal.c -o "$bin"
"$bin"
