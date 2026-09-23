#!/bin/bash
# REUSO DE CONEXAO em rede.c (23/09/2026): o handle por fio existia desde
# 5eb8bd2, mas o dlsym de curl_easy_reset nao, e toda chamada abria conexao
# nova. Aqui um servidor HTTP/1.1 local conta as conexoes TCP que recebe; o
# teste faz seis pedidos no mesmo fio (varios hosts nao cabem num teste sem
# rede, entao o cache e exercitado por uma sequencia de caminhos) e pede UMA
# conexao so. Um segundo fio tem de abrir a SUA — handle nao e compartilhado.
#   bash tests/rede_reuso.sh
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d /tmp/nuvio-rede-reuso.XXXXXX)
trap 'kill $SRV 2>/dev/null || true; rm -rf "$DIR"' EXIT
python3 - "$DIR/porta" <<'PY' &
import http.server, socketserver, sys, threading
conexoes = 0
trava = threading.Lock()
class H(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def log_message(self, *a): pass
    def setup(self):
        global conexoes
        with trava: conexoes += 1
        super().setup()
    def do_GET(self):
        corpo = (str(conexoes) if self.path == "/conexoes" else "x" * 2048).encode()
        self.send_response(200)
        self.send_header("Content-Length", str(len(corpo)))
        self.end_headers()
        self.wfile.write(corpo)
class S(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
s = S(("127.0.0.1", 0), H)
open(sys.argv[1], "w").write(str(s.server_address[1]))
s.serve_forever()
PY
SRV=$!
for i in $(seq 50); do [ -s "$DIR/porta" ] && break; sleep 0.1; done
cc -Isrc tests/rede_reuso.c src/rede.c src/redeurl.c -lpthread -o "$DIR/t" -O1 -g -Wall
"$DIR/t" "$(cat "$DIR/porta")"
