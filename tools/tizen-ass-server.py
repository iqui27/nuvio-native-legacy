#!/usr/bin/env python3
"""Servidor Range + CORS restrito à pasta da fixture ASS do harness."""
import http.server
import os
import socketserver
import sys
import threading
from urllib.parse import parse_qs, urlsplit

if len(sys.argv) != 2:
    raise SystemExit("uso: tools/tizen-ass-server.py <pasta-da-fixture>")

PASTA = os.path.realpath(sys.argv[1])
BIND = os.environ.get("NUVIO_ASS_BIND", "0.0.0.0")
contagem = 0
comando = ""
trava = threading.Lock()


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_):
        pass

    def _cors(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, HEAD, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Range, Content-Type")
        self.send_header("Access-Control-Expose-Headers", "Accept-Ranges, Content-Range, Content-Length")

    def do_OPTIONS(self):
        self.send_response(204)
        self._cors()
        self.send_header("Content-Length", "0")
        self.end_headers()

    def _path(self):
        nome = self.path.split("?", 1)[0].lstrip("/")
        if not nome or "/" in nome or nome in ("contagem", "zerar"):
            return None
        cam = os.path.realpath(os.path.join(PASTA, nome))
        return cam if os.path.dirname(cam) == PASTA else None

    def do_HEAD(self):
        cam = self._path()
        if not cam or not os.path.isfile(cam):
            self.send_error(404)
            return
        self.send_response(200)
        self._cors()
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Length", str(os.path.getsize(cam)))
        self.end_headers()

    def do_GET(self):
        global contagem, comando
        parsed = urlsplit(self.path)
        path = parsed.path
        if path == "/controle":
            acao = parse_qs(parsed.query).get("acao", [""])[0]
            if acao in ("embedded", "reset"):
                with trava:
                    comando = "" if acao == "reset" else acao
            with trava:
                body = comando.encode()
            self.send_response(200)
            self._cors()
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if path == "/contagem":
            with trava:
                body = str(contagem).encode()
            self.send_response(200)
            self._cors()
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if path == "/zerar":
            with trava:
                contagem = 0
            body = b"ok"
            self.send_response(200)
            self._cors()
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        cam = self._path()
        if not cam or not os.path.isfile(cam):
            self.send_error(404)
            return
        with trava:
            contagem += 1
        total = os.path.getsize(cam)
        inicio, fim = 0, total - 1
        parcial = False
        faixa = self.headers.get("Range", "")
        if faixa.startswith("bytes="):
            a, _, b = faixa[6:].partition("-")
            inicio = int(a) if a else 0
            fim = int(b) if b else total - 1
            fim = min(fim, total - 1)
            if inicio > fim or inicio >= total:
                self.send_response(416)
                self._cors()
                self.send_header("Content-Range", "bytes */%d" % total)
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            parcial = True
        n = fim - inicio + 1
        ext = os.path.splitext(cam)[1].lower()
        tipo = "text/plain; charset=utf-8" if ext == ".ass" else "video/x-matroska"
        self.send_response(206 if parcial else 200)
        self._cors()
        self.send_header("Accept-Ranges", "bytes")
        if parcial:
            self.send_header("Content-Range", "bytes %d-%d/%d" % (inicio, fim, total))
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(n))
        self.end_headers()
        with open(cam, "rb") as arq:
            arq.seek(inicio)
            restante = n
            while restante:
                bloco = arq.read(min(65536, restante))
                if not bloco:
                    break
                try:
                    self.wfile.write(bloco)
                except BrokenPipeError:
                    return
                restante -= len(bloco)


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def handle_error(self, *_):
        pass


PORT = int(os.environ.get("NUVIO_ASS_PORT", "0"))
srv = Server((BIND, PORT), Handler)
print("porta %d bind %s pasta-fixture" % (srv.server_address[1], BIND), flush=True)
srv.serve_forever()
