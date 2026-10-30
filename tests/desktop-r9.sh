#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-r9.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
bash tests/desktop-native-menu.sh
# Match the DMG's SDL2, not Homebrew's SDL2-to-SDL3 compatibility layer.
frameworks=/Volumes/ExternalSSD/tmp/rev/desktop/cache/frameworks
mkdir -p "$work/include/SDL2"
cp "$frameworks/SDL2.framework/Headers/"*.h "$work/include/SDL2/"
cc tests/desktop_native_events.m src/desktop_mac.m -o "$work/native-events" \
  -DNV_DESKTOP_UI -DNV_TOUCH_UI -Isrc -I"$work/include" -F"$frameworks" \
  -framework SDL2 -framework Cocoa -framework OpenGL -framework QuartzCore \
  -Wl,-rpath,"$frameworks" -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -Wl,-undefined,dynamic_lookup -Wno-deprecated-declarations
"$work/native-events"
python3 - <<'PY'
import re
from pathlib import Path
key='Clique para avaliar'
base=re.findall(r'^\s*\{ "((?:[^"\\]|\\.)*)",',Path('src/idioma_tab.h').read_text(),re.M)
assert base[base.index(key)-1] < key < base[base.index(key)+1]
index=base.index(key)
for path in Path('src').glob('idioma_*.h'):
    if path.name=='idioma_tab.h': continue
    keys=re.findall(r'^\s*T\("((?:[^"\\]|\\.)*)",',path.read_text(),re.M)
    assert keys==base, path
    assert keys[index]==key
print('PASS: rating translations aligned in every language')
PY
python3 - <<'PY'
from pathlib import Path
import ast
ast.parse(Path('tools/desktop-r9-capture.py').read_text())
PY
git diff --check
