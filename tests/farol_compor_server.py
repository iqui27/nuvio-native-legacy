#!/usr/bin/env python3
import http.server
import json
import os
import sys
import threading
import time
from urllib.parse import urlparse

if len(sys.argv) < 3:
    sys.stderr.write("usage: farol_compor_server.py <port_file> <capture_file>\n")
    sys.exit(2)

PORT_FILE = sys.argv[1]
CAPTURE_FILE = sys.argv[2]

ALLOWED_TOP_KEYS = {
    "v", "modo", "idioma", "hora", "gosto", "candidatos", "fatos",
    "infantil", "consentiu"
}
ALLOWED_GOSTO_KEYS = {
    "nivel", "fim_semana", "generos", "temas", "pessoas", "ancoras"
}
ALLOWED_CAND_KEYS = {
    "c", "id", "tipo", "titulo", "ano", "generos", "minutos", "nota",
    "pontos", "motivos", "eixos"
}
ALLOWED_MOTIVOS = {
    "FMO_TERMINOU", "FMO_GENERO", "FMO_PESSOA", "FMO_TEMA", "FMO_AMIGO",
    "FMO_SALVO", "FMO_CABE_HOJE", "FMO_REVER", "FMO_FORA_CURVA",
    "FMO_RECOMENDADO"
}

LOCK = threading.Lock()
STATE = {
    "scenario": "ok_home",
    "delay_s": 0.0,
    "hits": 0,
    "last_raw": b"",
    "last_perfil": "",
    "refresh_delay_s": 0.0,
    "refresh_started": 0,
    "refresh_hits": 0,
}

def validate_request(raw_bytes):
    if len(raw_bytes) > 12288:
        raise ValueError(f"body exceeds 12288 bytes: {len(raw_bytes)}")
    doc = json.loads(raw_bytes.decode("utf-8"))
    if not isinstance(doc, dict):
        raise ValueError("top level must be dict")
    extra = set(doc.keys()) - ALLOWED_TOP_KEYS
    if extra:
        raise ValueError(f"forbidden top keys: {extra}")
    if doc.get("v") != 1:
        raise ValueError(f"bad v: {doc.get('v')}")
    if doc.get("modo") not in ("home", "comparar", "dica", "retomada"):
        raise ValueError(f"bad modo: {doc.get('modo')}")
    hora = doc.get("hora")
    if not isinstance(hora, int) or hora < 0 or hora > 7:
        raise ValueError(f"bad hora bucket: {hora}")
    if doc.get("infantil") != 0 or doc.get("consentiu") != 1:
        raise ValueError(f"privacy flags invalid: infantil={doc.get('infantil')} consentiu={doc.get('consentiu')}")

    gosto = doc.get("gosto", {})
    if not isinstance(gosto, dict):
        raise ValueError("gosto must be dict")
    gextra = set(gosto.keys()) - ALLOWED_GOSTO_KEYS
    if gextra:
        raise ValueError(f"forbidden gosto keys: {gextra}")
    gens = gosto.get("generos", [])
    temas = gosto.get("temas", [])
    pessoas = gosto.get("pessoas", [])
    ancoras = gosto.get("ancoras", [])
    if len(gens) > 6 or len(temas) > 6 or len(pessoas) > 4 or len(ancoras) > 3:
        raise ValueError(f"taste bounds exceeded: g={len(gens)} t={len(temas)} p={len(pessoas)} a={len(ancoras)}")
    for lst in (gens, temas, pessoas):
        for item in lst:
            if not isinstance(item.get("id"), int) or item["id"] <= 0:
                raise ValueError(f"non-public taste id: {item}")

    cands = doc.get("candidatos", [])
    if len(cands) > 24:
        raise ValueError(f"too many candidates: {len(cands)}")
    anchor_ids = {a.get("id") for a in ancoras}
    cited = set()
    for i, c in enumerate(cands):
        cextra = set(c.keys()) - ALLOWED_CAND_KEYS
        if cextra:
            raise ValueError(f"forbidden cand keys: {cextra}")
        if c.get("c") != f"c{i}":
            raise ValueError(f"cN mismatch: {c.get('c')} vs c{i}")
        for gid in c.get("generos", []):
            if not isinstance(gid, int) or gid <= 0:
                raise ValueError(f"non-public cand genre: {gid}")
        for m in c.get("motivos", []):
            t = m.get("tipo")
            if t not in ALLOWED_MOTIVOS:
                raise ValueError(f"bad motivo: {t}")
            if t in ("FMO_AMIGO", "FMO_RECOMENDADO"):
                if m.get("a") != 0 or m.get("b") != 0 or "quem" in m or "nome" in m:
                    raise ValueError(f"friend motif not anonymized: {m}")
            if t == "FMO_TERMINOU":
                if m.get("id") not in anchor_ids:
                    raise ValueError(f"dangling FMO_TERMINOU anchor: {m.get('id')}")
                cited.add(m.get("id"))
    # PLANO-GOSTO 5.3: so ancora citada por motivo de candidato enviado.
    if anchor_ids - cited:
        raise ValueError(f"unreferenced anchors: {sorted(anchor_ids - cited)}")
    return doc

def build_response(scenario):
    if scenario == "ok_home":
        return 200, json.dumps({
            "v": 1,
            "tela": "home",
            "feita": 1795000000,
            "vale_s": 86400,
            "origem": "llm",
            "pecas": [
                {
                    "t": "fileira",
                    "id": "gosto1",
                    "titulo": {"x": "Noite de suspense direto", "m": "FT_FIL_PORQUE_TERMINOU", "a": ["tt11280740"]},
                    "sub": {"m": "FT_FIL_GENERO", "a": ["g53"]},
                    "forma": "cartaz",
                    "itens": ["c0", "c1", "c2", "c3", "c4", "c5"],
                    "motivo": {
                        "c0": {"x": "Clima tenso e curto para hoje", "m": "FT_MOT_CABE_HOJE", "a": ["92"]},
                        "c1": {"m": "FT_MOT_AMIGO_ANON"}
                    }
                },
                {
                    "t": "fileira",
                    "id": "gosto2",
                    "titulo": {"m": "FT_FIL_HORA"},
                    "forma": "larga",
                    "itens": ["c5", "c4", "c3", "c2", "c1", "c0"]
                },
                {
                    "t": "dica",
                    "id": "d_curto",
                    "l1": {"m": "FT_DICA_CURTO", "a": ["c0"]},
                    "acao": {"tipo": "abrir", "alvo": "c0"},
                    "icone": "aj_farol"
                }
            ]
        }, separators=(",", ":"))
    if scenario == "ok_home_2":
        return 200, json.dumps({
            "v": 1,
            "tela": "home",
            "origem": "llm",
            "pecas": [{
                "t": "fileira",
                "id": "gosto1",
                "titulo": {"x": "Nova selecao do perfil"},
                "forma": "cartaz",
                "itens": ["c1", "c2", "c3", "c4", "c5", "c0"]
            }]
        }, separators=(",", ":"))
    if scenario == "ok_comparar":
        return 200, json.dumps({
            "v": 1,
            "tela": "comparar",
            "origem": "llm",
            "pecas": [{
                "t": "comparar",
                "itens": ["c0", "c1"],
                "eixos": ["duracao", "nota", "porque"],
                "veredito": {"x": "Cabe inteiro antes de dormir", "m": "FT_CMP_VEREDITO_CABE", "a": ["c0"]},
                "escolha": "c0"
            }]
        }, separators=(",", ":"))
    if scenario == "ok_dica":
        return 200, json.dumps({
            "v": 1,
            "tela": "home",
            "origem": "llm",
            "pecas": [{
                "t": "dica",
                "id": "d_curto",
                "l1": {"m": "FT_DICA_CURTO", "a": ["c0"]},
                "acao": {"tipo": "abrir", "alvo": "c0"}
            }]
        }, separators=(",", ":"))
    if scenario == "ok_retomada":
        return 200, json.dumps({
            "v": 1,
            "tela": "detalhe",
            "origem": "llm",
            "pecas": [
                {
                    "t": "porque",
                    "alvo": "c0",
                    "confianca": 2,
                    "linhas": [{"x": "Combina com o seu gosto recente", "m": "FT_MOT_GENERO", "a": ["g53"]}],
                    "selos": [{"k": "genero", "id": 53}]
                },
                {
                    "t": "info",
                    "alvo": "c0",
                    "itens": [{"k": "tempo", "l": {"m": "FT_INFO_TERMINA", "a": ["23:30"]}}]
                }
            ]
        }, separators=(",", ":"))
    if scenario == "regras":
        return 200, '{"origem":"regras"}'
    if scenario == "malformed":
        return 200, '{"v":1,"tela":"home","pecas":[{'
    if scenario == "oversize":
        pad = "a" * 8200
        return 200, json.dumps({
            "v": 1,
            "tela": "home",
            "pecas": [{
                "t": "fileira",
                "id": "gosto1",
                "titulo": {"x": pad},
                "forma": "cartaz",
                "itens": ["c0", "c1", "c2", "c3", "c4", "c5"]
            }]
        })
    if scenario == "wrong_tela":
        return 200, json.dumps({
            "v": 1,
            "tela": "detalhe",
            "pecas": [{
                "t": "porque",
                "alvo": "c0",
                "confianca": 1,
                "linhas": [{"m": "FT_MOT_GENERO", "a": ["g53"]}]
            }]
        })
    if scenario == "bad_ref":
        return 200, json.dumps({
            "v": 1,
            "tela": "home",
            "pecas": [{
                "t": "fileira",
                "id": "gosto1",
                "titulo": {"m": "FT_FIL_HORA"},
                "forma": "cartaz",
                "itens": ["c0", "c1", "c2", "c3", "c4", "c99"]
            }]
        })
    if scenario == "status_429":
        return 429, '{"erro":"limite"}'
    if scenario == "status_500":
        return 500, '{"erro":"interno"}'
    if scenario == "status_401":
        return 401, '{"erro":"nao autorizado"}'
    return 500, '{"erro":"cenario desconhecido"}'

class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, format, *args):
        pass

    def _send(self, code, body):
        data = body.encode("utf-8") if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Connection", "close")
        self.end_headers()
        try:
            self.wfile.write(data)
        except BrokenPipeError:
            pass

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == "/__hits":
            with LOCK:
                out = {"hits": STATE["hits"], "scenario": STATE["scenario"]}
            self._send(200, json.dumps(out))
            return
        if parsed.path == "/__last_raw":
            with LOCK:
                raw = STATE["last_raw"]
            self._send(200, raw)
            return
        if parsed.path == "/__last_perfil":
            with LOCK:
                perfil = STATE["last_perfil"]
            self._send(200, perfil)
            return
        if parsed.path == "/__refresh_state":
            with LOCK:
                out = {
                    "started": STATE["refresh_started"],
                    "hits": STATE["refresh_hits"],
                }
            self._send(200, json.dumps(out))
            return
        self._send(404, '{"erro":"not found"}')

    def do_POST(self):
        parsed = urlparse(self.path)
        n = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(n)

        if parsed.path == "/__scenario":
            cfg = json.loads(raw.decode("utf-8"))
            with LOCK:
                if "scenario" in cfg:
                    STATE["scenario"] = cfg["scenario"]
                if "delay_s" in cfg:
                    STATE["delay_s"] = float(cfg["delay_s"])
                if cfg.get("reset_hits"):
                    STATE["hits"] = 0
                if "refresh_delay_s" in cfg:
                    STATE["refresh_delay_s"] = float(cfg["refresh_delay_s"])
                if cfg.get("reset_refresh"):
                    STATE["refresh_started"] = 0
                    STATE["refresh_hits"] = 0
            self._send(200, '{"ok":1}')
            return

        if parsed.path == "/auth/v1/token":
            with LOCK:
                STATE["refresh_started"] += 1
                delay_s = STATE["refresh_delay_s"]
            if delay_s > 0:
                step = 0.05
                waited = 0.0
                while waited < delay_s:
                    time.sleep(step)
                    waited += step
            with LOCK:
                STATE["refresh_hits"] += 1
            tok = "t0002.eyJzdWIiOiJ1MSIsImV4cCI6NDEwMjQ0NDgwMH0.s0002"
            self._send(200, json.dumps({
                "access_token": tok,
                "refresh_token": "r2",
                "token_type": "bearer",
            }))
            return

        if parsed.path.startswith("/rest/v1/rpc/"):
            expected = "Bearer t0002.eyJzdWIiOiJ1MSIsImV4cCI6NDEwMjQ0NDgwMH0.s0002"
            if self.headers.get("Authorization") == expected:
                self._send(200, "[]")
            else:
                self._send(401, '{"message":"JWT expired"}')
            return

        if parsed.path != "/v1/farol/compor":
            self._send(404, '{"erro":"not found"}')
            return

        with LOCK:
            STATE["hits"] += 1
            STATE["last_raw"] = raw
            STATE["last_perfil"] = self.headers.get("X-Nuvio-Perfil", "")
            sc = STATE["scenario"]
            delay_s = STATE["delay_s"]

        try:
            if not self.headers.get("Authorization", "").startswith("Bearer "):
                raise ValueError("missing Bearer Authorization header")
            if self.headers.get("X-Nuvio-Auth") not in ("nuvio", "trakt"):
                raise ValueError(f"bad X-Nuvio-Auth: {self.headers.get('X-Nuvio-Auth')}")
            doc = validate_request(raw)
        except Exception as e:
            with LOCK:
                with open(CAPTURE_FILE, "a", encoding="utf-8") as f:
                    f.write(json.dumps({"error": str(e), "raw": raw.decode("utf-8", "replace")}) + "\n")
            self._send(400, json.dumps({"erro": str(e)}))
            return

        with LOCK:
            with open(CAPTURE_FILE, "a", encoding="utf-8") as f:
                f.write(json.dumps({
                    "headers": {
                        "auth_present": bool(self.headers.get("Authorization")),
                        "via": self.headers.get("X-Nuvio-Auth", ""),
                        "perfil": self.headers.get("X-Nuvio-Perfil", ""),
                    },
                    "body": doc,
                    "raw_len": len(raw),
                }) + "\n")

        if delay_s > 0:
            step = 0.05
            waited = 0.0
            while waited < delay_s:
                time.sleep(step)
                waited += step

        code, body = build_response(sc)
        self._send(code, body)

class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

if __name__ == "__main__":
    srv = Server(("127.0.0.1", 0), Handler)
    port = srv.server_address[1]
    with open(PORT_FILE, "w", encoding="utf-8") as f:
        f.write(str(port))
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
