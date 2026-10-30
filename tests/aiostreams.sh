#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-aio.XXXXXX")
trap 'rm -rf "$work"' EXIT
if [ ! -f src/aiostreams.h ]; then echo 'FAIL: AIOStreams contract missing'; exit 1; fi
cc tests/aiostreams.c src/aiostreams.c src/js.c src/stream_parse.c src/fontefiltro.c src/linguas.c src/badges.c -Isrc -o "$work/test" -O1 -g \
  -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
"$work/test"
cc tests/aiostreams_ui.c src/aiostreams.c src/js.c src/jsw.c src/qr.c src/focus.c -Isrc -o "$work/ui" -O1 -g \
  -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2
"$work/ui"
python3 - <<'PY'
from pathlib import Path
source = Path('src/aiostreamsui.c').read_text()
workspace = source.split('static void workspace(void) {', 1)[1].split('if(pagina==TITULO)', 1)[0]
dashboard = workspace.index('dashboard();')
focused = workspace.index('if(lateral)', dashboard)
scrim = workspace.index('gfx_cor((GfxRect){72,148,1776,858}', focused)
assert 'T21_VEIL' in workspace[scrim:]
assert dashboard < focused < scrim < workspace.index('navigationCount=0; sidebar();', scrim)
print('aiostreams dashboard: scrim below opaque focused sidebar')
PY
