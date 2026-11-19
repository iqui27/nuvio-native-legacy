#!/bin/bash
# #221: a folha enche a cada addon e o automatico nao espera o mais lento.
# addons.c/rede.c/streams.c reais contra addons falsos com latencias
# diferentes num servidor HTTP local. Ver tests/fontes_parcial.c.
#
#   bash tests/fontes_parcial.sh            # so a logica
#   bash tests/fontes_parcial.sh /tmp/dir   # e grava capturas BMP da folha
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fontes-parcial.XXXXXX")
NUVIO_DADOS="$dir/dados"; mkdir -p "$NUVIO_DADOS"; export NUVIO_DADOS
cat > "$dir/addons.py" <<'PY'
import http.server, json, sys, time
LAT = {"Lento": (1.5, ["Lento 4K 2160p WEB-DL"]),
       "Rapido": (0.4, ["Rapido 1080p WEB-DL", "Rapido 1080p BluRay"]),
       "Tardio": (3.0, ["Tardio 720p HDTV"])}
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        nome = self.path.strip("/").split("/")[0]
        if nome == "Quebrado" or "/stream/" not in self.path:
            self.send_response(500); self.end_headers(); return
        espera, rot = LAT.get(nome, (0, []))
        time.sleep(espera)
        corpo = json.dumps({"streams": [
            {"name": r, "title": r + "\nfilme.mkv",
             "url": "http://127.0.0.1:1/%s/%d.mkv" % (nome, k)} for k, r in enumerate(rot)]}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(corpo)))
        self.end_headers(); self.wfile.write(corpo)
s = http.server.ThreadingHTTPServer(("127.0.0.1", 0), H)
print(s.server_address[1], flush=True)
s.serve_forever()
PY
python3 "$dir/addons.py" > "$dir/porta" &
srv=$!
trap 'kill $srv 2>/dev/null; rm -rf "$dir"' EXIT
for _ in $(seq 50); do [ -s "$dir/porta" ] && break; sleep 0.1; done
NV_PORTA=$(cat "$dir/porta"); export NV_PORTA
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
cc "${sources[@]}" tests/fontes_parcial.c -Isrc -o "$dir/teste" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
if [ $# -gt 0 ]; then mkdir -p "$1"; "$dir/teste" "$(cd "$1" && pwd)"; else "$dir/teste"; fi
