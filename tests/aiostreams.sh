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
cc tests/aiopair_tv.c src/aiostreams.c src/js.c src/jsw.c -Isrc -o "$work/pair" -O1 -g \
  -ffunction-sections -fdata-sections -Wl,-dead_strip
"$work/pair" > "$work/pair.log"
if rg -q 'synthetic-password|sample-uuid|Authorization:|instance.invalid' "$work/pair.log"; then
  echo 'FAIL: pairing logged sensitive data'; exit 1
fi
cat "$work/pair.log"
node tests/aiopair.mjs
cc tests/aiopair_qr.c src/qr.c -Isrc -o "$work/qr" -O1 -g \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -Wno-deprecated-declarations \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
"$work/qr"
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=localhost \
  -addext subjectAltName=DNS:localhost -keyout "$work/key.pem" -out "$work/cert.pem" > "$work/cert.log" 2>&1
cc tests/aiopair_live.c src/rede.c src/redeurl.c src/aiostreams.c src/js.c src/jsw.c -Isrc -o "$work/live" -O1 -g \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -pthread
node tests/aiopair_live.mjs "$work/live" "$work"
python3 - <<'PY'
from pathlib import Path
source = Path('src/aiostreamsui.c').read_text()
workspace = source.split('static void workspace(void) {', 1)[1].split('if(pagina==TITULO)', 1)[0]
assert 'dashboard();' in workspace and 'sidebar();' in workspace
assert 'T21_VEIL' not in workspace
assert 'float L=72+88+28+232*anim_clamp(sidebarOpen,0,1)' in source
assert '"aj_lock"' in source and 'dashboardCartao(c,(GfxRect){L+c*(tw+20),400,tw,110},1)' in source
assert 'Connect with UUID and password' not in source
print('aiostreams dashboard: persistent rail, content reflow, compact locked state')
PY
