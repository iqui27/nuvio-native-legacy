#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-cw-addon.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=gnu11 -O1 -g -pthread -Isrc -ffunction-sections -fdata-sections
       -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-macro-redefined)
if [[ $(uname -s) == Darwin ]]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc "${flags[@]}" tests/cw_addon_request.c src/idcanal.c src/js.c src/jsw.c src/watchstate.c src/addonstats.c src/fontefalha.c -o "$work/test"
"$work/test"
cc "${flags[@]}" tests/cw_addon_identity.c src/catalogo.c -o "$work/identity"
"$work/identity"
# Compile the actual router blocks with a cold catalog and delayed metadata.
python3 - "$work/cold-router.inc" <<'PY'
import pathlib, sys
s = pathlib.Path('src/app.c').read_text()
a = s.index('        const CatItem *cw = cat_item(it.indice);')
b = s.index('\n      }', s.index('        if (tocar) detail_pedir_reproduzir();', a))
c = s.index('  if (cwEspera) {', b)
d = s.index('\n\n  legendasDaFonte();', c)
pathlib.Path(sys.argv[1]).write_text(
    'static void cold_open(void) { struct { int indice; } it = {0};\n' + s[a:b] + '\n}\n'
    'static void cold_wait(void) {\n' + s[c:d] + '\n}\n')
PY
cc "${flags[@]}" -I"$work" tests/cw_addon_cold.c src/catalogo.c -o "$work/cold"
"$work/cold"
