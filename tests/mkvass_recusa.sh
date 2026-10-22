#!/bin/bash
# #385: host do video que RECUSA conexao (curl 7) com o video aberto. A leitura
# lateral (pre-busca do mkvass, colheita, sonda do MKV) tenta no maximo uma
# vez, pausa sem conexao nova e so volta ao host depois da pausa. Conta as
# tentativas num redirecionador local (302 para uma porta que recusa), como o
# StremThru -> no do CDN do TorBox do registro. Ver tests/mkvass_recusa.c.
#   bash tests/mkvass_recusa.sh
set -euo pipefail
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkvass-recusa.XXXXXX")
SRV=""
trap 'if [ -n "$SRV" ]; then kill "$SRV" 2>/dev/null || true; fi; rm -rf "$DIR"' EXIT

cat > "$DIR/servidor.py" <<'PY'
import http.server, socket, socketserver, sys, threading
# Porta que RECUSA: livre e fechada (RST -> ECONNREFUSED, curl 7). Um socket
# preso sem listen nao serve: no macOS ele descarta o SYN e vira curl 28.
fechado = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
fechado.bind(("127.0.0.1", 0))
PORTA_FECHADA = fechado.getsockname()[1]
fechado.close()
trava = threading.Lock()
estado = {"modo": "recusa", "contagem": 0}
class H(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def log_message(self, *a): pass
    def texto(self, t):
        b = t.encode()
        self.send_response(200); self.send_header("Content-Length", str(len(b))); self.end_headers()
        self.wfile.write(b)
    def do_HEAD(self): self.do_GET()
    def do_GET(self):
        p = self.path
        if p == "/contagem":
            with trava: self.texto(str(estado["contagem"])); return
        if p == "/zerar":
            with trava: estado["contagem"] = 0
            self.texto("0"); return
        if p.startswith("/modo/"):
            with trava: estado["modo"] = p[6:]
            self.texto("0"); return
        if p.startswith("/stream/"):
            with trava:
                estado["contagem"] += 1
                modo = estado["modo"]
            if modo == "recusa":
                self.send_response(302)
                self.send_header("Location", "http://127.0.0.1:%d%s" % (PORTA_FECHADA, p))
                self.send_header("Content-Length", "0"); self.end_headers(); return
            # aceita: 206 com zeros (nao e MKV) — o que importa e o host atender
            rng = self.headers.get("Range", "bytes=0-65535")
            a, b = rng.split("=")[1].split("-")
            a = int(a); b = int(b) if b else a + 65535
            n = b - a + 1
            self.send_response(206)
            self.send_header("Content-Range", "bytes %d-%d/%d" % (a, b, 10 * 1024 * 1024))
            self.send_header("Content-Length", str(n)); self.end_headers()
            self.wfile.write(b"\0" * n); return
        self.send_response(404); self.send_header("Content-Length", "0"); self.end_headers()
class S(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
s = S(("127.0.0.1", 0), H)
print("porta", s.server_address[1], flush=True)
s.serve_forever()
PY
python3 "$DIR/servidor.py" > "$DIR/porta.txt" &
SRV=$!
for _ in $(seq 1 50); do grep -q porta "$DIR/porta.txt" 2>/dev/null && break; sleep 0.1; done
PORTA=$(awk '/porta/{print $2}' "$DIR/porta.txt")
[ -n "$PORTA" ] || { echo "mkvass_recusa.sh: servidor nao subiu"; exit 1; }

# Pausas encurtadas (-D), como no tests/mkvass.sh: na TV 10-60 s e 0,5-8 s.
cc -Isrc -I/opt/homebrew/include -O1 -g -Wall -Wno-deprecated-declarations \
  -DMKVASS_PAUSA_CDN_INI_MS=150L -DMKVASS_PAUSA_CDN_MAX_MS=600L \
  -DMKVASS_RECUO_INI_MS=20L -DMKVASS_RECUO_MAX_MS=160L \
  -DREDE_CALMA_INI_MS=600UL -DREDE_CALMA_MAX_MS=2400UL \
  tests/mkvass_recusa.c src/mkvass.c src/assrender.c src/legenda.c src/rede.c src/redeurl.c \
  src/dados.c src/mkv.c -o "$DIR/test" -lpthread
mkdir "$DIR/dados"
set +e
NUVIO_DADOS="$DIR/dados" "$DIR/test" "http://127.0.0.1:$PORTA" > "$DIR/saida.txt" 2>&1
rc=$?
set -e
grep -vE '^\[(legenda|ass)' "$DIR/saida.txt" | sed -E 's#http://127\.0\.0\.1:[0-9]+#<local>#g'
# A linha que a TV vai mostrar: a recusa vira pausa, sem a segunda conexao.
if ! grep -q 'recusou conexao' "$DIR/saida.txt"; then
  echo "mkvass_recusa.sh: falta a linha de pausa por conexao recusada"; rc=1
fi
if grep -q 'de novo em conexao nova' "$DIR/saida.txt"; then
  echo "mkvass_recusa.sh: a leitura lateral repetiu 'em conexao nova' contra o host que recusou"; rc=1
fi
exit $rc
