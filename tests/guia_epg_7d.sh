#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
grep -q 'EPG_JANELA_FUTURO  (7 \* 86400)' src/epg.c
grep -q 'ajustes_guia_dias() \* 86400' src/epg.c
grep -q 'dst->sinopse' src/guia.c
echo "guia epg 7d: ok"
