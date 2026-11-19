#!/bin/bash
# Executa a implementacao real isolando apenas as passadas de desenho.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-menu-borda.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work/menu_vidro_borda_impl.h" <<'PY'
from pathlib import Path
import sys
s = Path('src/gfx.c').read_text()
a = s.index('void gfx_vidro_borda(')
b = s.index('\n}', a) + 2
Path(sys.argv[1]).write_text(s[a:b] + '\n')
PY
cc tests/menu_vidro_borda.c -Isrc -I"$work" -I/opt/homebrew/include \
  -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -o "$work/teste"
"$work/teste"
