#!/bin/bash
# J4: sheet body never scales with the island; minimum GPU effects never cut island body motion.
set -eu
cd "$(dirname "$0")/.."
fail=0
if grep -n 'k=fminf(1,fminf(g.w' src/filmo.c; then echo "FAIL: sheet text scales with island"; fail=1; fi
blk=$(sed -n '/O CORPO (ilha_corpo): a mola/,/vis = alvo != M_RELOGIO/p' src/ilha.c)
if grep -q efeitos_minimos <<<"$blk"; then echo "FAIL: minimum effects cut corpo spring"; fail=1; fi
if grep -n 'ajustes_animacoes_reduzidas() || gfx_efeitos_minimos())' src/ilha.c; then echo "FAIL: minimum effects cut body fade"; fail=1; fi
if grep -n 'folhaDx=g.x' src/filmo.c; then echo "FAIL: sheet content slides with island edge"; fail=1; fi
grep -q 'rev=fminf(1,fmaxf(0,(cob' src/filmo.c || { echo "FAIL: content not revealed once it fits"; fail=1; }
if grep -n 'foco_anel' src/filmo.c; then echo "FAIL: touched poster ring uses flagged helper"; fail=1; fi
body=$(sed -n '/^static void corpoFilmo/,/^}/p' src/filmo.c)
if grep -q 'TXT_INI' <<<"$body"; then echo "FAIL: sheet body adds a second text-scale layer inside the island"; fail=1; fi
grep -q 'r.x+r.w-(fim.x+fim.w)' <<<"$body" || { echo "FAIL: sheet content not pinned to island rect"; fail=1; }
grep -q 'ESCALA_INI();GfxRect sheet' src/filmo.c || { echo "FAIL: sheet size not measured in the island layer"; fail=1; }
[ $fail = 0 ] && echo "j4 sheet motion: PASS"
exit $fail
