#!/bin/bash
# Glass standard (docs/issues/vidro-padrao.md): every panel/popup/dialog/island
# paints its body through the shared gfx_vidro family (one backdrop per frame),
# never with its own dark translucent gfx_cor rect. Grep-based; no link.
#
#   bash tests/vidro_padrao.sh
set -u
cd "$(dirname "$0")/.."
fail=0

# 1) No file may draw a dark translucent panel body (rgb .03-.09, alpha >= .5) with
#    gfx_cor, except solid fallbacks (else branches) and lines marked `vidro-ok: why`.
pat='gfx_cor\([^;]*[ (,]0?\.0[3-9][0-9]*f?, ?0?\.0[3-9][0-9]*f?, ?0?\.0[3-9][0-9]*f?, ?0?\.[5-9][0-9]*f?'
for f in src/*.c src/*.h src/*.inc; do
  n=$(grep -E "$pat" "$f" | grep -vcE "else|vidro-ok" || true)
  [ "$n" -eq 0 ] && continue
  if [ "$n" -gt 0 ]; then
    echo "FAIL $f: $n own translucent glass rect(s)"; grep -nE "$pat" "$f" | grep -vE "else|vidro-ok" | cut -c1-140; fail=1
  fi
done

# 2) Known surfaces must call the shared helper (file|regex).
need='
src/plrui.c|gfx_vidro_material
src/telemetria.c|gfx_vidro_material
src/atualizacao.c|gfx_vidro_material
src/teclado.c|gfx_vidro_material
src/celbotao.c|gfx_vidro_material
src/novidades170.c|gfx_vidro_ou_solido
src/ajustes_ux_desenho.inc|gfx_vidro_material
src/ilha.c|gfx_vidro_material
src/menu.c|gfx_vidro\(
src/guia.c|gfx_vidro_folha
src/guia.c|gfx_vidro_ou_solido
src/home.c|gfx_vidro_ou_solido
src/badges.c|gfx_vidro_ou_solido
src/vertudo.c|gfx_vidro_ou_solido
src/avisos.c|gfx_vidro_ou_solido
src/novidades.c|gfx_vidro_ou_solido
src/diagnostico.c|gfx_vidro_ou_solido
src/telefonecartao.h|gfx_vidro_ou_solido
src/streams.c|gfx_vidro_folha
src/amigosfil.c|gfx_vidro_folha
src/ctxmenu.c|gfx_vidro_folha
src/agendaui.c|gfx_vidro_folha
src/plrilha.c|plrui_material
'
while IFS='|' read -r f re; do
  [ -z "$f" ] && continue
  grep -qE "$re" "$f" || { echo "FAIL $f: missing $re"; fail=1; }
done <<< "$need"

# 3) No opaque base may sit under a glass sheet (it hides the frost): quick menu
#    (ctxmenu.c ilhaCtx), agenda panel. Parental guide goes via plrilha -> plrui_material.
if grep -n "opaque base under the glass sheet" src/*.c; then echo "FAIL: opaque base under glass"; fail=1; fi

[ "$fail" -eq 0 ] && echo "vidro padrao: PASS"
exit $fail
