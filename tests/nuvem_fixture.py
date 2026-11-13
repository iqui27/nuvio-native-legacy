#!/usr/bin/env python3
# Fixture local, SINTETICO e descartavel do servidor de conta (Supabase-like),
# so para testes. Semeia usuario, perfis e addons de mentira para que o ciclo
# nuvem/sessao/sync real do app rode INTEIRO no loopback. Qualquer metodo ou
# caminho fora da lista abaixo fica registrado como violacao em DIR/log.jsonl
# e quem chama deve falhar se houver violacao. O log so ve pedidos que CHEGAM
# aqui: nao prova ausencia de pedido para outro host. Nenhum
# dado real e lido ou devolvido; os tokens sao JWTs de mentira com sub/exp.
#
#   python3 tests/nuvem_fixture.py --port 0 --state DIR
#
# --port 0 escolhe uma porta livre e grava {"port": N} em DIR/port.json.
# Cada pedido e anexado a DIR/log.jsonl (metodo, caminho, host, se conhecido).
# So escuta em 127.0.0.1; sem proxy e sem upstream.
import argparse, base64, json, os, sys, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

USUARIO = "fixture-user-00000000-0000-4000-8000-000000000001"
BASE_URL = None
LOGF = None
VIOL = []

def b64(s):
    return base64.urlsafe_b64encode(s.encode()).rstrip(b"=").decode()

def jwt():
    exp = int(time.time()) + 86400
    return "%s.%s.%s" % (
        b64('{"alg":"none","typ":"JWT"}'),
        b64('{"sub":"%s","exp":%d,"aud":"authenticated"}' % (USUARIO, exp)),
        b64("fixture"))

TOK_ACESSO = jwt()
TOK_RENOV = jwt() + "-refresh"

def resposta_auth():
    return {"access_token": TOK_ACESSO, "token_type": "bearer",
            "expires_in": 86400, "refresh_token": TOK_RENOV,
            "user": {"id": USUARIO, "aud": "authenticated", "role": "authenticated"}}

PERFIS = [{"profile_index": 1, "id": 1, "name": "Fixture",
           "avatar_id": "avatar_lalo", "avatar_url": "",
           "is_primary": True, "has_pin": False,
           "uses_primary_plugins": True, "uses_primary_addons": True}]

def addons_rows():
    return [{"user_id": USUARIO, "profile_id": 1, "sort_order": 0,
             "name": "FixtureAddons", "enabled": True,
             "url": "%s/addon/manifest.json" % BASE_URL}]

MANIFESTO = {"id": "fixture.addons", "version": "0.0.1",
             "name": "Fixture Addons", "description": "addon sintetico",
             "resources": ["catalog"], "types": ["movie"],
             "catalogs": [{"type": "movie", "id": "fixture",
                           "name": "Fixture"}],
             "idPrefixes": ["tt"]}

CATALOGO = {"metas": []}

# (metodo, prefixo do caminho) -> funcao -> (status, corpo). Caminho inclui
# a query — o que interessa e o prefixo; a query dos pulls e livre.
ROTAS = [
    ("POST", "/auth/v1/signup",   lambda q, b: (200, resposta_auth())),
    ("POST", "/auth/v1/token",    lambda q, b: (200, resposta_auth())),
    ("POST", "/auth/v1/logout",   lambda q, b: (204, None)),
    ("GET",  "/rest/v1/addons",   lambda q, b: (200, addons_rows())),
    ("GET",  "/rest/v1/plugins",  lambda q, b: (200, [])),
    ("PATCH","/rest/v1/addons",   lambda q, b: (200, addons_rows())),
    ("GET",  "/addon/manifest.json", lambda q, b: (200, MANIFESTO)),
]

RPCS = {
    "get_sync_owner": lambda b: USUARIO,   # _mandar json-encodes once -> "uuid"
    "sync_pull_profiles": lambda b: PERFIS,
    "sync_pull_profile_locks": lambda b: [],
    "sync_pull_profile_settings_blob": lambda b: [{"settings_json": {"version": 1, "features": {}}}],
    "sync_push_profile_settings_blob": lambda b: {"ok": True},
    "sync_pull_home_catalog_settings": lambda b: [],
    "sync_pull_collections": lambda b: [],
    "sync_pull_library": lambda b: [],
    "sync_push_library": lambda b: {"ok": True},
    "sync_pull_watched_items": lambda b: [],
    "sync_push_watched_items": lambda b: {"ok": True},
    "sync_delete_watched_items": lambda b: {"ok": True},
    "sync_pull_provider_credentials": lambda b: [],
    "sync_push_provider_credentials": lambda b: {"ok": True},
    "sync_push_plugins": lambda b: {"ok": True},
    "sync_push_profiles": lambda b: {"ok": True},
    "sync_delete_profile_data": lambda b: {"ok": True},
    "verify_profile_pin": lambda b: False,
    "get_avatar_catalog": lambda b: [],
    "sync_push_progress": lambda b: {"ok": True},
    "sync_pull_progress": lambda b: [],
    "sync_pull_watch_progress": lambda b: [],
}

class H(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a): pass

    def _registrar(self, metodo, conhecido):
        host = self.headers.get("Host", "")
        rec = {"metodo": metodo, "caminho": self.path, "host": host,
               "conhecido": bool(conhecido)}
        if not conhecido:
            VIOL.append(rec)
            rec["violacao"] = True
        with open(LOGF, "a") as f:
            f.write(json.dumps(rec) + "\n")

    def _mandar(self, st, corpo):
        if corpo is None:
            self.send_response(st)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        dados = corpo if isinstance(corpo, bytes) else json.dumps(corpo).encode()
        self.send_response(st)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(dados)))
        self.end_headers()
        self.wfile.write(dados)

    def _tratar(self, metodo):
        n = int(self.headers.get("Content-Length") or 0)
        corpo = self.rfile.read(n) if n else b""
        caminho = self.path.split("?", 1)[0]
        try:
            body = json.loads(corpo) if corpo else {}
        except Exception:
            body = {}
        if caminho.startswith("/rest/v1/rpc/"):
            nome = caminho[len("/rest/v1/rpc/"):]
            f = RPCS.get(nome)
            if f and metodo == "POST":
                self._registrar(metodo, True)
                self._mandar(200, f(body))
                return
            self._registrar(metodo, False)
            self._mandar(404, {"message": "funcao desconhecida no fixture",
                               "code": "PGRST202"})
            return
        for m, pref, f in ROTAS:
            if metodo == m and caminho == pref:
                self._registrar(metodo, True)
                st, resp = f(self.path, body)
                self._mandar(st, resp)
                return
        # Qualquer caminho fora da fixture: registrado como violacao.
        self._registrar(metodo, False)
        self._mandar(404, {"message": "caminho desconhecido no fixture"})

    def do_GET(self):    self._tratar("GET")
    def do_POST(self):   self._tratar("POST")
    def do_PATCH(self):  self._tratar("PATCH")
    def do_PUT(self):    self._tratar("PUT")
    def do_DELETE(self): self._tratar("DELETE")

def main():
    global BASE_URL, LOGF
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--state", required=True)
    a = ap.parse_args()
    os.makedirs(a.state, exist_ok=True)
    LOGF = os.path.join(a.state, "log.jsonl")
    srv = ThreadingHTTPServer(("127.0.0.1", a.port), H)
    porta = srv.server_address[1]
    BASE_URL = "http://127.0.0.1:%d" % porta
    with open(os.path.join(a.state, "port.json"), "w") as f:
        json.dump({"port": porta}, f)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    main()
