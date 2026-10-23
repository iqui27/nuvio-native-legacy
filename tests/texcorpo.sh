#!/bin/bash
# 200 COM CORPO VAZIO NAO E ARTE (#205): o servidor local conta as batidas —
# /vazia200 falha e entra em recuo (uma batida so nos dois pedidos), e a
# vizinha boa baixa normal. Compila o tex_cache real (sem duble de
# rede_baixar_bin) contra o rede.c real: e o caminho medido do aparelho.
#   bash tests/texcorpo.sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-texcorpo.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$tmp"' EXIT
python3 - "$tmp/porta" "$tmp/batidas" <<'PY' &
import http.server, socketserver, sys
class H(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def log_message(self, *a): pass
    def do_GET(self):
        with open(sys.argv[2], "a") as f: f.write(self.path + "\n")
        corpo = (b"\xff\xd8" + b"x" * 1022) if self.path == "/boa.jpg" else b""
        self.send_response(200)
        self.send_header("Content-Type", "image/jpeg")
        self.send_header("Content-Length", str(len(corpo)))
        self.end_headers()
        if corpo:
            try: self.wfile.write(corpo)
            except (BrokenPipeError, ConnectionResetError): pass
class S(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
s = S(("127.0.0.1", 0), H)
open(sys.argv[1], "w").write(str(s.server_address[1]))
s.serve_forever()
PY
srv=$!
for i in $(seq 50); do [ -s "$tmp/porta" ] && break; sleep 0.1; done
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/tex_cache.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/texcorpo.c -Isrc -o "$tmp/teste" \
  -O1 -g -Wall -Wextra -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$tmp/teste" "$(cat "$tmp/porta")" "$tmp/batidas"