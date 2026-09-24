#!/bin/bash
# CONEXAO PARADA em rede.c (24/09/2026). Na C9 a arte da Apple TV passou a
# estourar o prazo (curl 28) no app enquanto o curl da propria TV baixava o
# mesmo JPEG em 0,29 s. Este teste monta, num servidor HTTP/1.1 local, os
# jeitos de uma conexao parar sem avisar e confere que rede.c nao espera o
# prazo inteiro por eles:
#   A. conexao reusada que o outro lado abandonou em silencio (sem FIN: NAT
#      que esqueceu a entrada, servidor que sumiu) depois de ficar ociosa;
#   B. corpo que para no meio, com a conexao aberta;
#   C. servidor que fecha a conexao ociosa COM FIN (a libcurl ja percebe);
#   D. rajada de pedidos seguidos continua numa conexao so (o ganho do reuso).
# O servidor conta conexoes e "congela" as que o teste manda congelar: le o
# pedido e nunca responde nem fecha. Conexao nova funciona normalmente.
#   bash tests/rede_parada.sh
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d /tmp/nuvio-rede-parada.XXXXXX)
trap 'kill $SRV 2>/dev/null || true; rm -rf "$DIR"' EXIT
python3 - "$DIR/porta" <<'PY' &
import socket, socketserver, sys, threading, time
conexoes = 0
cortes = {}
trava = threading.Lock()
CORPO = b"x" * 65536
class H(socketserver.StreamRequestHandler):
    def handle(self):
        global conexoes
        with trava: conexoes += 1
        congelada = False
        while True:
            linha = self.rfile.readline()
            if not linha: return
            caminho = linha.split(b" ")[1].decode() if b" " in linha else "/"
            while True:
                h = self.rfile.readline()
                if not h or h in (b"\r\n", b"\n"): break
            if congelada:
                # Le e some: nem resposta nem FIN, como um par que morreu.
                time.sleep(60); return
            if caminho.startswith("/conexoes"):
                corpo = str(conexoes).encode()
            elif caminho.startswith("/corte/"):
                with trava:
                    n = cortes.get(caminho, 0); cortes[caminho] = n + 1
                if n == 0:
                    # Cabecalho e um quarto do corpo, depois silencio.
                    self.wfile.write(b"HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n" % len(CORPO))
                    self.wfile.write(CORPO[:16384]); self.wfile.flush()
                    time.sleep(60); return
                corpo = CORPO
            else:
                corpo = CORPO
            self.wfile.write(b"HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n" % len(corpo) + corpo)
            self.wfile.flush()
            if "congelar=1" in caminho: congelada = True
            if "fechar=1" in caminho:
                time.sleep(0.2)
                self.connection.shutdown(socket.SHUT_RDWR); return
class S(socketserver.ThreadingMixIn, socketserver.TCPServer):
    daemon_threads = True
    allow_reuse_address = True
    def handle_error(self, *a): pass   # cliente que desistiu da congelada
s = S(("127.0.0.1", 0), H)
open(sys.argv[1], "w").write(str(s.server_address[1]))
s.serve_forever()
PY
SRV=$!
for i in $(seq 50); do [ -s "$DIR/porta" ] && break; sleep 0.1; done
cc -Isrc tests/rede_parada.c src/rede.c src/redeurl.c -lpthread -o "$DIR/t" -O1 -g -Wall
# Limite de ociosidade curto so para o teste nao demorar (o padrao e 20 s).
NUVIO_REDE_OCIOSO_MS=1000 "$DIR/t" "$(cat "$DIR/porta")"
