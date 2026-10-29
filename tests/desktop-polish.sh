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
bash tests/desktop_ui_syntax.sh
cc -fsyntax-only -DNV_DESKTOP_UI -DNV_TOUCH_UI -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 src/detail.c src/vertudo.c src/guiav5ui.c src/ctxmenu.c src/main.c src/amigosfil.c -Wno-deprecated-declarations
python3 - <<'PY'
import ast
from pathlib import Path
for name in ("desktop-polish-capture.py","desktop-polish-proof.py"):
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
assert "setDisableActions:YES" in native and "height-1-y" in native
assert "SDL_WINDOW_HIDDEN" in Path("src/main.c").read_text()
assert "if (getenv(\"NV_CAPTURA\"))" in Path("src/main.c").read_text()
PY
cc -fsyntax-only -DNV_DESKTOP_UI -DNV_TOUCH_UI -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 src/desktop_mac.m -Wno-deprecated-declarations
git diff --check
