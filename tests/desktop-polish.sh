#!/bin/bash
# Focused desktop behavior, native bridge and shared-platform syntax gate.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-desktop-polish.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
bash tests/desktop_ui.sh
bash tests/desktop.sh
flags=(-DNV_TOUCH_UI -DNV_DESKTOP_UI -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -ffunction-sections -fdata-sections -Wl,-dead_strip)
cc tests/desktop_input.c "${flags[@]}" -L/opt/homebrew/lib -lSDL2 -o "$work/input"
"$work/input"
cc tests/desktop_search.c src/buscanorm.c "${flags[@]}" -Wl,-undefined,dynamic_lookup -o "$work/search"
"$work/search"
cc tests/desktop_filmo.c "${flags[@]}" -Wl,-undefined,dynamic_lookup -L/opt/homebrew/lib -lSDL2 -o "$work/filmo"
"$work/filmo"
for mode in desktop tv; do
  detail_flags=()
  if [ "$mode" = desktop ]; then detail_flags=(-DNV_TOUCH_UI -DNV_DESKTOP_UI); fi
  cc tests/desktop_detail.c "${detail_flags[@]}" -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -ffunction-sections -fdata-sections -Wl,-dead_strip -Wl,-undefined,dynamic_lookup -o "$work/detail-$mode"
  "$work/detail-$mode"
done
bash tests/desktop_ui_syntax.sh
cc -fsyntax-only -DNV_DESKTOP_UI -DNV_TOUCH_UI -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 src/detail.c src/vertudo.c src/guiav5ui.c src/ctxmenu.c src/main.c src/amigosfil.c -Wno-deprecated-declarations
python3 - <<'PY'
import ast
from pathlib import Path
for name in ("desktop-polish-capture.py","desktop-polish-proof.py","desktop-r6-capture.py","desktop-r6-proof.py"):
    ast.parse((Path("tools")/name).read_text())
capture = Path("tools/desktop-polish-capture.py").read_text()
assert "NV_CAPTURA='1'" in capture and "SDL_MAC_BACKGROUND_APP='1'" in capture
assert "osascript" not in capture and "desktop-pointer" not in capture
assert "process.terminate()" in capture and "process.kill()" in capture
native = Path("src/desktop_mac.m").read_text()
assert "NSApplicationActivationPolicyAccessory" in native and "orderBack:nil" in native
assert "NSGlassEffectView" in native and "NSVisualEffectMaterialSidebar" in native
assert "content.layer.masksToBounds = YES" in native
assert "CATransform3DMakeScale" not in native
assert "setDisableActions:YES" in native
assert "sidebarBackdrop" not in native and "glReadPixels" not in native
assert "NSOpenGLContextParameterSurfaceOpacity" in native
assert "NSVisualEffectBlendingModeBehindWindow" in native
assert "sidebar.alphaValue=sidebarReveal" in native
menu=Path("src/menu.c").read_text()
toolbar=menu.split("void menu_desktop_busca_desenhar(void) {",1)[1].split("static float desktopLayout",1)[0]
assert "ponteiro_alvo" not in toolbar and "mt_biblioteca" not in toolbar and "MENU_BUSCAR" not in toolbar
app=Path("src/app.c").read_text()
assert app.index("if (!ctx_aberto()) menu_desktop_busca_desenhar();") < app.index("    menu_desenhar(agora);",app.index("if (!ctx_aberto()) menu_desktop_busca_desenhar();"))
assert "menu_desktop_foco()" in native and "sidebarLanguage!=ajustes_idioma()" in native
assert 'button.image.template=YES' in native and 'button.contentTintColor=NSColor.systemBlueColor' in native
assert '@"Browse"' not in native and '@"Services"' not in native
app=Path('src/app.c').read_text()
route=app.split('if (!strcmp(screen,"filmography"))',1)[1].split('detail_fechar_seco()',1)[0]
assert 'detail_mostrar_pessoa' in route and 'vertudo_colecao' not in route
detail=Path('src/detail.c').read_text()
assert 'lastWidth!=NV_TELA_W || lastHeight!=NV_TELA_H' in detail
assert 'if(pessoaAberta) { filmo_evento(e); return; }' in detail
assert 'filmo_desenhar(a);return;' in detail
assert "SDL_WINDOW_HIDDEN" in Path("src/main.c").read_text()
assert 'SDL_GL_SetSwapInterval(getenv("NV_CAPTURA") ? 0 : 1)' in Path('src/main.c').read_text()
assert "if (getenv(\"NV_CAPTURA\"))" in Path("src/main.c").read_text()
PY
cc -fsyntax-only -DNV_DESKTOP_UI -DNV_TOUCH_UI -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 src/desktop_mac.m -Wno-deprecated-declarations
git diff --check
