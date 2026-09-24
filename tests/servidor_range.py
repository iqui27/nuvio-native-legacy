#!/usr/bin/env python3
# Mini servidor HTTP COM Range, para os testes que leem MKV por trecho
# (tests/mkvass.sh). Existe porque `python3 -m http.server` responde 200 com o
# arquivo inteiro a qualquer Range, e o que se quer medir e justamente quantos
# bytes um pedido parcial traz.
#
#   python3 tests/servidor_range.py <pasta>
#
# Imprime "porta N" na primeira linha e serve ate ser morto.
#   GET /<arquivo>            -> 206 com Content-Range quando ha Range; 200 sem
#   GET /norange/<arquivo>    -> IGNORA o Range e devolve 200 com tudo (o caso
#                                do servidor que nao sabe Range, para o no-go)
#   GET /falhaAaB/<arquivo> -> 503 do A-esimo ao B-esimo GET desde /zerar (falha
#                                PASSAGEIRA: rede/5xx no meio da colheita)
#   GET /limite1/<arquivo>    -> 429 quando ja ha OUTRO GET do mesmo modo em
#                                andamento (CDN de debrid que aceita uma
#                                conexao por link, #92)
#   GET /redir/<resto>        -> 307 para /<resto> (link de addon que
#                                redireciona ao CDN, #92)
#   GET /contagem             -> numero de GETs a arquivos ate agora (texto)
#   GET /contagem429          -> quantos 429 o /limite1 respondeu
#   GET /contagemredir        -> quantos 307 o /redir respondeu
#   GET /zerar                -> zera as contagens
import http.server, os, re, socketserver, sys, threading

PASTA = sys.argv[1]
BIND = os.environ.get("NUVIO_RANGE_BIND", "127.0.0.1")
contagem = 0
curtoPedidos = 0
ativos1 = 0
recusas429 = 0
redirs = 0
trava = threading.Lock()

class H(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def log_message(self, *a): pass

    def _arquivo(self):
        nome = self.path.lstrip("/")
        semRange = nome.startswith("norange/")
        if semRange: nome = nome[len("norange/"):]
        self.limite1 = nome.startswith("limite1/")
        if self.limite1: nome = nome[len("limite1/"):]
        lento = nome.startswith("lento/")
        if lento: nome = nome[len("lento/"):]
        curtoCues = nome.startswith("curtocues/")
        if curtoCues: nome = nome[len("curtocues/"):]
        curto = nome.startswith("curto/")
        if curto: nome = nome[len("curto/"):]
        m = re.match(r"falha(\d+)a(\d+)/", nome)
        self.falha = (int(m.group(1)), int(m.group(2))) if m else None
        if m: nome = nome[m.end():]
        cam = os.path.join(PASTA, os.path.basename(nome))
        return cam, semRange, lento, curto, curtoCues

    def do_HEAD(self):
        cam, _, _, _, _ = self._arquivo()
        if not os.path.isfile(cam): self.send_error(404); return
        self.send_response(200)
        self.send_header("Content-Length", str(os.path.getsize(cam)))
        self.send_header("Accept-Ranges", "bytes")
        self.end_headers()

    def _texto(self, v):
        corpo = ("%d" % v).encode()
        self.send_response(200); self.send_header("Content-Length", str(len(corpo)))
        self.end_headers(); self.wfile.write(corpo)

    def do_GET(self):
        global ativos1
        if self.path.startswith("/redir/"):
            global redirs
            with trava: redirs += 1
            self.send_response(307)
            self.send_header("Location", self.path[len("/redir"):])
            self.send_header("Content-Length", "0"); self.end_headers(); return
        if self.path.startswith("/limite1/"):
            with trava: ativos1 += 1
            try: self._get()
            finally:
                with trava: ativos1 -= 1
            return
        self._get()

    def _get(self):
        global contagem, curtoPedidos, recusas429, redirs
        if self.path == "/contagem":
            with trava: v = contagem
            self._texto(v); return
        if self.path == "/contagem429":
            with trava: v = recusas429
            self._texto(v); return
        if self.path == "/contagemredir":
            with trava: v = redirs
            self._texto(v); return
        if self.path == "/zerar":
            with trava:
                contagem = 0
                curtoPedidos = 0
                recusas429 = 0
                redirs = 0
            self.send_response(200); self.send_header("Content-Length", "2")
            self.end_headers(); self.wfile.write(b"ok"); return
        cam, semRange, lento, curto, curtoCues = self._arquivo()
        if not os.path.isfile(cam): self.send_error(404); return
        with trava:
            contagem += 1
            if curto:
                curtoPedidos += 1
            curtoN = curtoPedidos
            k = contagem
        if self.limite1:
            with trava:
                recusa = ativos1 > 1
                if recusa: recusas429 += 1
            if recusa:
                self.send_response(429); self.send_header("Content-Length", "0")
                self.end_headers(); return
        if self.falha and self.falha[0] <= k <= self.falha[1]:
            self.send_response(503); self.send_header("Content-Length", "0")
            self.end_headers(); return
        total = os.path.getsize(cam)
        rng = self.headers.get("Range")
        ini, fim = 0, total - 1
        parcial = False
        if rng and rng.startswith("bytes=") and not semRange:
            a, _, b = rng[6:].partition("-")
            ini = int(a) if a else 0
            fim = int(b) if b else total - 1
            if fim >= total: fim = total - 1
            if ini > fim or ini >= total:
                self.send_response(416); self.send_header("Content-Range", "bytes */%d" % total)
                self.send_header("Content-Length", "0"); self.end_headers(); return
            parcial = True
        if curto and os.environ.get("NUVIO_RANGE_TRACE") == "1":
            print("range-trace curto #%d %s bytes=%d-%d" %
                  (curtoN, self.path, ini, fim), file=sys.stderr, flush=True)
        if lento:
            import time
            time.sleep(1.2)
        # Preserva o cabecalho e o indice (os primeiros pedidos do worker),
        # truncando somente a primeira janela de Cluster. Assim o teste
        # exercita a resposta 206 curta no ponto em que ela realmente deixa
        # cues ausentes, em vez de transformar o MKV inteiro em um cabecalho
        # incompleto.
        if curto and parcial and curtoN >= 6:
            fim = min(fim, ini + 31)
        if curtoCues and parcial and ini > total - 16 * 1024 and fim - ini + 1 > 32:
            fim = min(fim, ini + 31)
        n = fim - ini + 1
        self.send_response(206 if parcial else 200)
        if parcial: self.send_header("Content-Range", "bytes %d-%d/%d" % (ini, fim, total))
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Length", str(n))
        self.send_header("Content-Type", "application/octet-stream")
        self.end_headers()
        with open(cam, "rb") as f:
            f.seek(ini)
            resta = n
            while resta > 0:
                pedaco = f.read(min(65536, resta))
                if not pedaco: break
                try: self.wfile.write(pedaco)
                except BrokenPipeError: return   # o cliente cortou no teto: esperado
                resta -= len(pedaco)

class Servidor(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True
    # O cliente corta a conexao de proposito quando o teto do rede_baixar_trecho
    # enche (servidor que ignora Range): nao e erro do teste, e ruido no log.
    def handle_error(self, request, client_address): pass

srv = Servidor((BIND, 0), H)
print("porta %d bind %s" % (srv.server_address[1], BIND), flush=True)
srv.serve_forever()
