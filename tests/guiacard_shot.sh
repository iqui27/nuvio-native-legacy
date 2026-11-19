#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=${1:-/Volumes/ExternalSSD/tmp/rev/guiacard/after}
mkdir -p "$out"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guiacard-shot.XXXXXX")
trap '[[ -z ${logo_server:-} ]] || kill "$logo_server" 2>/dev/null; rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/dados"
mkdir -p "$NUVIO_DADOS"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c) continue;; esac
  sources+=("$source")
done
extra=()
if [[ ${GUIACARD_BASELINE:-0} == 1 ]]; then
  git show b8e9007bc6ac7de1dee8d3a2f351de111e7223c7:src/guiav5ui.c > "$work/guiav5ui.c"
  extra=(-DGUIAV5_UNDER_TEST=\"$work/guiav5ui.c\")
fi
cc "${sources[@]}" tests/guiacard_shot.c ${extra[@]+"${extra[@]}"} -Isrc -o "$work/shot" -O0 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations
python3 - "$work/port" <<'PY' &
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import sys
class Handler(SimpleHTTPRequestHandler):
    def log_message(self, *args): pass
server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
Path(sys.argv[1]).write_text(str(server.server_port))
server.serve_forever()
PY
logo_server=$!
for ((i=0;i<100;i++)); do [[ -s "$work/port" ]] && break; sleep .05; done
read -r port < "$work/port" || [[ -n ${port:-} ]]
export GUIACARD_LOGO_URL="http://127.0.0.1:$port/tests/fixtures/aovivo/logo.png"
"$work/shot" "$out"
sips -c 256 1824 --cropOffset 168 48 "$out/preview-playing-app.png" --out "$out/card.png" >/dev/null
sips -c 76 320 --cropOffset 606 48 "$out/preview-playing-app.png" --out "$out/logo.png" >/dev/null
sips -c 76 320 --cropOffset 692 48 "$out/preview-playing-app.png" --out "$out/no-logo.png" >/dev/null
