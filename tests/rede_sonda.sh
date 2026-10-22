#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rede-sonda.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$tmp"' EXIT
python3 - "$tmp/porta" <<'PY' &
import http.server, socketserver, sys, time
class H(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def log_message(self, *args): pass
    def do_GET(self):
        caminho = self.path.split("?")[0]
        if caminho in ("/redirect", "/ref-redirect", "/long"):
            alvo = "/referer" if caminho == "/ref-redirect" else "/media"
            if caminho == "/long": alvo += "?q=" + "x" * 2000
            self.send_response(302); self.send_header("Location", alvo)
            self.send_header("Content-Length", "512"); self.end_headers()
            try: self.wfile.write(b"r" * 512)
            except (BrokenPipeError, ConnectionResetError): pass
            return
        if caminho == "/delay": time.sleep(2)
        status = 200
        corpo = b"m" * 64
        if caminho == "/erro500": status = 500
        elif caminho == "/referer":
            status = 200 if self.headers.get("Referer") == "https://media.example/" else 403
        elif caminho == "/partial": status = 206
        elif caminho == "/large": corpo = b"m" * (256 * 1024)
        elif caminho == "/truncated": corpo = b"m" * 7
        tipo = None
        if caminho == "/html": corpo = b"<html>" + b"x" * 300; tipo = "text/html; charset=UTF-8"
        elif caminho == "/json": corpo = b'{"e":1}'; tipo = "application/json"
        elif caminho == "/video": tipo = "video/mp4"
        self.send_response(status)
        if tipo: self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", "1024" if caminho == "/truncated" else str(len(corpo)))
        self.end_headers()
        try: self.wfile.write(corpo); self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError): pass
        if caminho == "/truncated": self.close_connection = True
class S(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    def handle_error(self, *args): pass
s = S(("127.0.0.1", 0), H)
with open(sys.argv[1], "w") as f: f.write(str(s.server_address[1]))
s.serve_forever()
PY
srv=$!
for i in $(seq 50); do [ -s "$tmp/porta" ] && break; sleep 0.1; done
flags=(-O1 -g -Wall -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -ffunction-sections -fdata-sections -Wl,-dead_strip -Wno-deprecated-declarations -pthread)
if [ "${NV_SANITIZERS:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/rede_sonda.c src/rede.c src/redeurl.c src/streams.c src/naovideo.c -o "$tmp/teste"
"$tmp/teste" "http://127.0.0.1:$(cat "$tmp/porta")"
cc "${flags[@]}" -D__EMSCRIPTEN__ tests/streams_sonda_wgt.c src/streams.c src/naovideo.c -o "$tmp/wgt"
"$tmp/wgt"
node tests/rede_sonda_wgt.cjs
