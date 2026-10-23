#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

# Focused syntax gate for the three surfaces in this change; no link or full app build.
cc -fsyntax-only src/guia.c src/streams.c src/recenviar.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined

# Both Guide drawers use the configured Glass leaf and retain an opaque path.
[ "$(rg -c 'materialPainelGuia\(p,' src/guia.c)" -eq 2 ]
rg -q 'if \(ajustes_vidro\(\)\) gfx_vidro_folha\(p, raio, a\)' src/guia.c
rg -q 'else gfx_cor\(p, raio, 0\.071f, 0\.075f, 0\.086f, 0\.98f \* a\)' src/guia.c

# The Sources sheet uses adjustable opacity/Frost in Glass mode and the shared
# opaque material when Glass is off. Send-to-friend rows use the poster-menu focus surface.
rg -q 'gfx_vidro_folha\(corpo,FOLHA_RAIO_IL/corpo.h,anim\)' src/streams.c
rg -q 'else plrui_material\(corpo,FOLHA_RAIO_IL,0,anim\)' src/streams.c
rg -q '^  plrui_material\(p, RE_RAIO, 1, a\);' src/recenviar.c
rg -q 'gfx_vidro_painel\(b, 14\.0f / RE_COD_H, 0\.55f, a\)' src/recenviar.c
rg -q 'plrui_linha_foco\(r, h \* \.5f, k \* a\)' src/recenviar.c

# The row style/order menu uses the shared Glass leaf (follows glass opacity),
# with no opaque base under it; the solid island stays for Glass off.
rg -q '^    gfx_vidro_folha\(m, 30\.0f / m\.h, a\);' src/ctxmenu.c
rg -q '^  \} else ilhaCtx\(m, 30\.0f, a\);' src/ctxmenu.c

echo 'glass surface contract: PASS'
