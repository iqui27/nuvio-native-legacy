#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -D_DARWIN_C_SOURCE -Isrc src/addonmodo.c tests/addonguia.c -o /tmp/nuvio-addonguia-test
/tmp/nuvio-addonguia-test
# Footer actions (no AIO addon) must read the list focus channel 50+f, not focoAnim[0].
grep -q 'botaoEm(i18n(acao),f==addons_n()?"aj_rotate-cw":NULL,228,820,50+f' src/addonguia.c
# Sheets keep the borderless island glass (no rim) and frost is on in the shot harnesses.
grep -q 'gfx_vidro(r, raio, GFX_VIDRO_ILHA' src/tela21.h
! grep -q 'gfx_vidro_material' src/tela21.h
grep -q 'assert(ajustes_vidro_fosco())' tests/addonguia_shot.c tests/aiostreams_shot.c
# Island glass captures the backdrop before its body tint is painted (r4 P1-1).
awk '/gfx_vidro\(r, raio, GFX_VIDRO_ILHA/{v=NR} /gfx_cor\(r, raio, .055f, .059f, .071f, .91f/{c=NR} END{exit !(v && c > v)}' src/tela21.h
# Glass pills/chips inside an island draw no rim (r4 P1-2).
grep -q 'semAro = tipo == GFX_VIDRO_ILHA || vidroNaIlha(tela)' src/gfx.c
grep -q 'if (!vidroNaIlha(tela)) gfx_vidro_aro' src/gfx.c
