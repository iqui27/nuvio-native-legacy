#!/bin/bash
# TESTE DE VELOCIDADE do diagnostico: a conta (src/vazao.c) e a medida
# (rede_medir_vazao em src/rede.c) contra um servidor HTTP local com Range e
# vazao LIMITADA, sem rede de fora:
#   /lento     arquivo "de 1 GB" (nada alocado) a 1 MB/s, com Range 206/416
#   /redir     302 para /lento (o endereco final volta para quem chama)
#   /parado    64 KB e silencio com a conexao aberta
#   /proibido  403 com pagina de erro
#   /curto     200 KB inteiros, sem Range
#   /addon/stream/movie/<id>.json  seis fontes (3 mediveis, 2 hosts: 127.0.0.1
#              e localhost; torrent, aviso e fora de cache); /sem-fontes/ 404
# e depois o FLUXO da tela (tests/vazao_fluxo.c): botao, fio, resultado,
# relatorio sem url/host e o Voltar no meio, com a janela encurtada para 2 s.
# O Tizen (XHR em pedacos) nao roda aqui: precisa do navegador da TV.
#   bash tests/vazao.sh
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d /tmp/nuvio-vazao.XXXXXX)
SRV=
trap 'if [ -n "$SRV" ]; then kill $SRV 2>/dev/null || true; fi; rm -rf "$DIR"' EXIT

cc -Isrc tests/vazao.c src/vazao.c src/rede.c src/redeurl.c -lpthread \
  -o "$DIR/t" -O1 -g -Wall -Wextra

python3 - "$DIR/porta" <<'PY' &
import sys, time, re, http.server, socketserver
TAM = 1024 * 1024 * 1024
PEDACO = 16384
class H(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def log_message(self, *a): pass
    def lento(self):
        ini, fim = 0, TAM - 1
        m = re.match(r"bytes=(\d+)-(\d*)", self.headers.get("Range", ""))
        if m:
            ini = int(m.group(1))
            if m.group(2): fim = min(int(m.group(2)), TAM - 1)
            if ini >= TAM:
                self.send_response(416)
                self.send_header("Content-Range", "bytes */%d" % TAM)
                self.send_header("Content-Length", "0"); self.end_headers(); return
            self.send_response(206)
            self.send_header("Content-Range", "bytes %d-%d/%d" % (ini, fim, TAM))
        else:
            self.send_response(200)
        self.send_header("Content-Length", str(fim - ini + 1)); self.end_headers()
        bloco = b"v" * PEDACO
        falta = fim - ini + 1
        try:
            # 1 MB/s: 64 pedacos de 16 KB por segundo, no relogio (sem deriva).
            t0, k = time.monotonic(), 0
            while falta > 0:
                n = min(PEDACO, falta)
                self.wfile.write(bloco[:n]); falta -= n; k += 1
                espera = t0 + k / 64.0 - time.monotonic()
                if espera > 0: time.sleep(espera)
        except (BrokenPipeError, ConnectionResetError):
            pass
    def do_GET(self):
        c = self.path
        if c.startswith("/lento"): return self.lento()
        if c.startswith("/addon/stream/movie/"):
            p = self.server.server_address[1]
            a, b = "http://127.0.0.1:%d" % p, "http://localhost:%d" % p
            corpo = ('{"streams":['
                     '{"name":"4K","title":"Filme 2160p","url":"%s/lento/a.mkv"},'
                     '{"name":"1080p","title":"Filme 1080p","url":"%s/lento/b.mkv"},'
                     '{"name":"720p","title":"Filme 720p","url":"%s/lento/c.mkv"},'
                     '{"name":"Torrent 4K","infoHash":"0123456789abcdef0123456789abcdef01234567"},'
                     '{"name":"4K DV","title":"aviso 2160p","url":"%s/x/slate.mp4"},'
                     '{"name":"\u23f3 4K","title":"fora 2160p","url":"%s/lento/d.mkv"}'
                     ']}' % (a, b, a, a, a)).encode()
            self.send_response(200); self.send_header("Content-Length", str(len(corpo)))
            self.end_headers(); self.wfile.write(corpo); return
        if c.startswith("/redir"):
            self.send_response(302); self.send_header("Location", "/lento")
            self.send_header("Content-Length", "0"); self.end_headers(); return
        if c.startswith("/parado"):
            self.send_response(200); self.send_header("Content-Length", str(10 * 1024 * 1024))
            self.end_headers()
            try:
                self.wfile.write(b"p" * 65536); self.wfile.flush(); time.sleep(30)
            except (BrokenPipeError, ConnectionResetError):
                pass
            return
        if c.startswith("/proibido"):
            corpo = b"<html>403</html>" * 64
            self.send_response(403); self.send_header("Content-Length", str(len(corpo)))
            self.end_headers(); self.wfile.write(corpo); return
        if c.startswith("/curto"):
            corpo = b"c" * 200000
            self.send_response(200); self.send_header("Content-Length", str(len(corpo)))
            self.end_headers(); self.wfile.write(corpo); return
        self.send_response(404); self.send_header("Content-Length", "0"); self.end_headers()
class S(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True
    def handle_error(self, *a): pass   # cliente que cortou de proposito
s = S(("127.0.0.1", 0), H)
open(sys.argv[1], "w").write(str(s.server_address[1]))
s.serve_forever()
PY
SRV=$!
for i in $(seq 50); do [ -s "$DIR/porta" ] && break; sleep 0.1; done
"$DIR/t" "$(cat "$DIR/porta")"

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/diagnostico.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/vazao_fluxo.c -Isrc -o "$DIR/fluxo" -DVAZ_JANELA_S=2 \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$DIR/fluxo" "$(cat "$DIR/porta")" | grep -E '^(ok|\[vazao\])'
echo "vazao: tudo ok"
