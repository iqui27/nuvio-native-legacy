#!/bin/bash
# #709: cap mkvass parallel ranges on TPK (Tizen 6 anime ASS)
set -eu
cd "$(dirname "$0")/.."
grep -q 'MKVASS_PARALELOS_TPK60' src/mkvass.c
grep -q '#709' src/mkvass.c
grep -q 'f->paralelos > MKVASS_PARALELOS_TPK60' src/mkvass.c
cc -std=gnu11 -O1 -DNV_MKVFIX_SEM_REDE -DNV_TPK -Isrc -c -o /tmp/mkvass709.o src/mkvass.c 2>/dev/null || cc -std=gnu11 -O1 -DNV_TPK -Isrc -fsyntax-only src/mkvass.c
echo "mkvass_709_tpk: PASS"
