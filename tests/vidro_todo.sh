#!/bin/bash
# Vidro em tudo: the shared materials follow "Opacidade do vidro" and the panels
# share one tone (tests/vidro_todo.c), and every inventoried surface calls one
# of them (grep below; table in docs/issues/vidro-todo.md).
#   bash tests/vidro_todo.sh            (RAIZ=<other checkout> runs it on that tree)
set -u
cd "$(dirname "$0")/.."
t=$PWD/tests
raiz=${RAIZ:-$PWD}
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-vidro-todo.XXXXXX")
trap 'rm -rf "$work"' EXIT
fail=0

# 1) Every inventoried surface draws through the shared material (file|regex).
need='
src/ilha.c|if \(repouso\) gfx_chrome\(R, raio, A\)
src/ilha.c|else if \(ajustes_vidro\(\)\) gfx_vidro_material\(R, raio, fundo, A\)
src/menu.c|if \(topo\(\)\) gfx_chrome\(r, raio, a\)
src/menu.c|gfx_vidro_material\(r, raio, al, a\)
src/menu.c|gfx_vidro_material\(R, raioPx / R.h, TV_MIOLO_MIN, A\)
src/recenviar.c|^  plrui_material\(p, RE_RAIO, 1, a\);
src/homevazio.c|gfx_vidro_material\(p, 0.06f, GFX_VIDRO_DENS, alfa\)
src/vertudo.c|gfx_vidro_material\(\(GfxRect\)\{px, py, pw, ph\}
src/plrui.c|if \(ajustes_vidro\(\)\) gfx_vidro_painel\(r, 0.5f, 0.55f, a\)
src/estatvivoui.c|if \(ajustes_vidro\(\)\) gfx_vidro_painel\(r, 18.0f / r.h
src/amigoperfil.c|if \(ajustes_vidro\(\)\) gfx_vidro_painel\(r, 18.0f / r.h
'
while IFS='|' read -r f re re2; do
  [ -z "$f" ] && continue
  grep -qE "$re${re2:+|$re2}" "$raiz/$f" || { echo "FAIL $f: missing $re"; fail=1; }
done <<< "$need"
# One edge light: gfx_vidro (panel) and gfx_vidro_chip draw gfx_vidro_luz; the
# surfaces below only draw their own corner light with glass off.
grep -qE '^  if \(tipo == GFX_VIDRO_PAINEL\) gfx_vidro_luz\(r, raio, a\);' "$raiz/src/gfx.c" &&
  grep -qE '^  gfx_vidro_luz\(r, raio, a\);' "$raiz/src/gfx.c" || { echo "FAIL gfx.c: shared edge light"; fail=1; }
for f in plrui atualizacao ctxmenu agendaui salvospainel celbotao avisos agendaviso dvtelaui guia; do
  grep -n "gfx_luz_canto(" "$raiz/src/$f.c" | grep -vE "if \(!(vid|ajustes_vidro\(\))\) gfx_luz_canto" |
    grep -E "$(case $f in guia) echo 'pv, raio, pv.w|p, 28.0f';; *) echo .;; esac)" && { echo "FAIL $f: own edge light on glass"; fail=1; }
done
grep -qF 'if (tipoLuz >= 0 || (!repouso && !ajustes_vidro()))' "$raiz/src/ilha.c" || { echo "FAIL ilha.c: own white light on glass"; fail=1; }
grep -nE 'gfx_brilho_topo\(R, raioPx' "$raiz/src/menu.c" && { echo "FAIL menu.c: sidebar own top light"; fail=1; }
# Chips, badges, focus backings and progress tracks: glass through gfx_vidro_chip.
for f in plrui badges streams home guia episodios posplay detail biblioteca salvospainel ilha agendaui; do
  grep -q "gfx_vidro_chip(" "$raiz/src/$f.c" || { echo "FAIL $f: no gfx_vidro_chip"; fail=1; }
done
# The two private panel tones are gone.
grep -nE "GFX_VIDRO_PAINEL, (0\.09f|15\.0f / 255\.0f)" "$raiz"/src/*.c && { echo "FAIL private glass tone"; fail=1; }

# 2) Runtime: opacity and tone (GL, hidden window).
sources=()
for source in "$raiz"/src/*.c "$raiz"/src/dts/*.c; do
  [ "$source" != "$raiz/src/main.c" ] && sources+=("$source")
done
if cc "${sources[@]}" "$t/vidro_todo.c" -I"$raiz/src" -o "$work/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined >"$work/cc.log" 2>&1; then
  mkdir -p "$work/dados"
  (cd "$raiz" && NUVIO_DADOS="$work/dados" NUVIO_TESTE_DIR="$work/dados" "$work/test") || fail=1
else
  cat "$work/cc.log" >&2; fail=1
fi

[ "$fail" -eq 0 ] && echo "vidro todo: PASS" || echo "vidro todo: FAIL"
exit $fail
