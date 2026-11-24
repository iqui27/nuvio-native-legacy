"""Servidor local do teste do envio do Farol (issue 18).

O prefixo do caminho escolhe o cenario; /stats?p=<prefixo> conta os pedidos
e confere o pacote recebido (v == 1, <= 35 000 B, fp == sha256(fingerprint)[:16]).
Uso: python3 tests/farol_envio_server.py <arquivo-da-porta>
"""
import hashlib
import http.server
import json
import socketserver
import sys
import threading
import time
from urllib.parse import parse_qs, urlparse

TRAVA = threading.Lock()
STATS = {}
CODIGO = "4F7K92"
CODIGO2 = "8HJ3MX"


def stats(p):
    return STATS.setdefault(p, {
        "get": 0, "post": 0, "auth_ok": 1, "fp_query": "", "fp_body": "", "fp_ok": 0,
        "v_ok": 0, "bytes": 0, "linha_n": -1, "linha_ult_b": -1, "c": "", "pl": "", "ver": "",
        "so": "",
    })


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def _json(self, status, obj):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def _rota(self):
        u = urlparse(self.path)
        for fim in ("/v1/farol/conhecido", "/v1/farol/registro"):
            if u.path.endswith(fim):
                return u.path[: -len(fim)], fim, u
        return None, None, u

    def _auth(self, st):
        ok = (self.headers.get("Authorization") == "Bearer tok-farol-18" and
              self.headers.get("X-Nuvio-Auth") == "nuvio")
        if not ok:
            st["auth_ok"] = 0
        return ok

    def do_GET(self):
        u = urlparse(self.path)
        if u.path == "/stats":
            p = parse_qs(u.query).get("p", [""])[0]
            with TRAVA:
                self._json(200, dict(stats(p)))
            return
        pref, rota, u = self._rota()
        if rota != "/v1/farol/conhecido":
            self._json(404, {"erro": "nao encontrado"})
            return
        q = parse_qs(u.query)
        with TRAVA:
            st = stats(pref)
            st["get"] += 1
            st["fp_query"] = q.get("fp", [""])[0]
            st["c"] = q.get("c", [""])[0]
            st["pl"] = q.get("p", [""])[0]
            st["ver"] = q.get("v", [""])[0]
            st["so"] = q.get("so_maior", [""])[0]
            auth = self._auth(st)
        if pref == "/lento":
            time.sleep(10)
        if not auth:
            self._json(401, {"erro": "nao autenticado"})
            return
        if pref == "/corrigido":
            self._json(200, {"ok": 1, "conhecido": {
                "estado": "corrigido", "versao": "2.1.3",
                "nota_pt": "Corrigido na versão 2.1.3 — atualize",
                "nota_en": "Fixed in 2.1.3"}})
            return
        if pref == "/conget500":
            self._json(500, {"erro": "x"})
            return
        if pref == "/okcon":
            # `conhecido` sem correcao: o envio segue.
            self._json(200, {"ok": 1, "conhecido": {"estado": "contorno", "versao": "",
                                                     "nota_pt": "Use AC3", "nota_en": "Use AC3"}})
            return
        self._json(200, {"ok": 1, "conhecido": None})

    def do_POST(self):
        pref, rota, _ = self._rota()
        n = int(self.headers.get("Content-Length", "0") or 0)
        raw = self.rfile.read(n) if n > 0 else b""
        if rota != "/v1/farol/registro":
            self._json(404, {"erro": "nao encontrado"})
            return
        with TRAVA:
            st = stats(pref)
            st["post"] += 1
            st["bytes"] = len(raw)
            auth = self._auth(st)
            try:
                obj = json.loads(raw.decode("utf-8"))
                er = obj.get("erro", {})
                st["v_ok"] = 1 if (obj.get("v") == 1 and len(raw) <= 35000) else 0
                st["fp_body"] = er.get("fp", "")
                fpc = hashlib.sha256(er.get("fingerprint", "").encode()).hexdigest()[:16]
                st["fp_ok"] = 1 if fpc == st["fp_body"] else 0
                linha = obj.get("linha", [])
                st["linha_n"] = len(linha) if isinstance(linha, list) else -1
                if isinstance(linha, list) and linha and isinstance(linha[-1], dict):
                    st["linha_ult_b"] = int(linha[-1].get("b", -1))
            except Exception:
                st["v_ok"] = 0
        if pref == "/lento":
            time.sleep(10)
        if not auth:
            self._json(401, {"erro": "nao autenticado"})
            return
        if pref == "/r429":
            self._json(429, {"erro": "limite"})
        elif pref == "/r422":
            self._json(422, {"erro": "pacote fora do esquema"})
        elif pref == "/r500":
            self._json(503, {"erro": "farol indisponivel"})
        elif pref == "/semcod":
            self._json(200, {"ok": 1})
        elif pref == "/okcon":
            self._json(200, {"ok": 1, "codigo": CODIGO2, "vence": 1,
                             "conhecido": {"estado": "contorno", "versao": "",
                                           "nota_pt": "Use AC3", "nota_en": "Use AC3"}})
        else:
            self._json(200, {"ok": 1, "codigo": CODIGO, "vence": int(time.time()) + 30 * 86400})


class Servidor(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


def main():
    srv = Servidor(("127.0.0.1", 0), Handler)
    with open(sys.argv[1] + ".tmp", "w") as f:
        f.write(str(srv.server_address[1]))
    import os
    os.replace(sys.argv[1] + ".tmp", sys.argv[1])
    srv.serve_forever()


if __name__ == "__main__":
    main()
