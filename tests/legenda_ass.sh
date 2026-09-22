#!/bin/bash
# Parser ASS/SSA (#92). So legenda.c + adaptador ASS + stub de rede: sem SDL,
# sem rede.
#   bash tests/legenda_ass.sh
set -eu
cd "$(dirname "$0")/.."
cc src/assrender.c src/legenda.c tests/legenda_ass.c -Isrc -o /tmp/nuvio-legenda-ass -O1 -g \
  -Wall -Wno-deprecated-declarations
/tmp/nuvio-legenda-ass
