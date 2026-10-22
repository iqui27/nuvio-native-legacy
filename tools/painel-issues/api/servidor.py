#!/usr/bin/env python3
"""API minima das respostas do dono no painel de issues (so biblioteca padrao).

  POST /api/respostas  {"id","resposta":"sim"|"nao","nota"?}  (o horario e do servidor)
  GET  /api/respostas  -> {"versao":1,"respostas":{id:{"historico":[...]}}}
  GET  /api/saude

Sem autenticacao: o painel e so LAN/Tailscale, como o resto. A defesa e o
cabecalho Origin (se vier, tem de ser um dos origens do painel) e o
Content-Type application/json (obriga preflight em outro site).

Config por ambiente (ou argumentos de mesmo nome em minusculas):
  PAINEL_DATA_JSON   data.json publicado (ids validos)  [/data/html/data.json]
  PAINEL_RESPOSTAS   arquivo de respostas               [/data/respostas/respostas.json]
  PAINEL_ORIGENS     origens aceitos, separados por virgula
  PAINEL_HOST/PORTA  [0.0.0.0] [8000]
"""
import argparse, datetime, fcntl, json, os, re, sys, tempfile, threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

MAX_NOTA = 2000
MAX_CORPO = 16384  # 2000 chars em \uXXXX = 12 KB; 8 KB rejeitaria nota valida
MAX_HISTORICO = 200
RE_ID = re.compile(r"^dec-[a-z0-9]+(-[a-z0-9]+)*$")
ORIGENS_PADRAO = "http://192.168.1.20:8094,http://100.77.116.81:8094"
TRAVA = threading.Lock()


class Erro(Exception):
    def __init__(self, codigo, msg):
        self.codigo, self.msg = codigo, msg


def agora():
    return datetime.datetime.now().astimezone().isoformat(timespec="seconds")


def decisoes_publicadas(caminho):
    """id -> decisao, lido a cada pedido (o publicar.sh troca o arquivo)."""
    try:
        with open(caminho, encoding="utf-8") as f:
            d = json.load(f)
        return {x["id"]: x for x in d.get("decisoes", []) if isinstance(x, dict) and "id" in x}
    except (OSError, ValueError):
        raise Erro(503, "data.json indisponivel")


def ler(caminho):
    try:
        with open(caminho, encoding="utf-8") as f:
            d = json.load(f)
    except FileNotFoundError:
        return {"versao": 1, "respostas": {}}
    except ValueError:
        raise Erro(500, "respostas.json corrompido; nada foi sobrescrito")
    if not isinstance(d, dict) or not isinstance(d.get("respostas"), dict):
        raise Erro(500, "respostas.json em formato inesperado")
    return d


def gravar(caminho, dados):
    pasta = os.path.dirname(os.path.abspath(caminho))
    fd, tmp = tempfile.mkstemp(prefix=".respostas.", suffix=".tmp", dir=pasta)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            json.dump(dados, f, ensure_ascii=False, indent=1, sort_keys=True)
            f.write("\n")
            f.flush()
            os.fsync(f.fileno())
        os.chmod(tmp, 0o644)
        os.replace(tmp, caminho)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise
    try:  # o replace ja valeu: falha aqui so vai para o log, a escrita nao e desfeita
        dfd = os.open(pasta, os.O_RDONLY)
        try:
            os.fsync(dfd)
        finally:
            os.close(dfd)
    except OSError as e:
        print("aviso: fsync da pasta falhou: %s" % e, file=sys.stderr, flush=True)


def prontidao(cfg):
    """None se pronto; senao o motivo (vira 503 em /api/saude)."""
    try:
        if not decisoes_publicadas(cfg["data_json"]):
            return "data.json sem ids de decisao"
    except Erro as e:
        return e.msg
    pasta = os.path.dirname(os.path.abspath(cfg["respostas"]))
    try:  # nome unico por chamada (mkstemp: aleatorio); remove so o proprio arquivo
        fd, tmp = tempfile.mkstemp(prefix=".saude.", dir=pasta)
        os.close(fd)
        try:
            os.unlink(tmp)
        except FileNotFoundError:
            pass
    except OSError:
        return "pasta de respostas nao gravavel"
    try:
        ler(cfg["respostas"])
    except Erro as e:
        return e.msg
    return None


def registrar(cfg, corpo):
    if not isinstance(corpo, dict):
        raise Erro(400, "corpo deve ser um objeto JSON")
    did, resp = corpo.get("id"), corpo.get("resposta")
    nota = corpo.get("nota", "")
    if not isinstance(did, str) or not RE_ID.match(did) or len(did) > 80:
        raise Erro(400, "id invalido")
    if resp not in ("sim", "nao"):
        raise Erro(400, "resposta deve ser 'sim' ou 'nao'")
    if nota is None:
        nota = ""
    if not isinstance(nota, str) or len(nota) > MAX_NOTA:
        raise Erro(400, "nota deve ser texto de ate %d caracteres" % MAX_NOTA)
    dec = decisoes_publicadas(cfg["data_json"]).get(did)
    if dec is None:
        raise Erro(404, "decisao desconhecida")
    if dec.get("aplicada_em"):
        raise Erro(409, "decisao ja aplicada; nao aceita mais resposta")
    reg = {"resposta": resp, "nota": nota, "quando": agora()}
    with TRAVA:
        with open(cfg["respostas"] + ".lock", "a") as lk:
            fcntl.flock(lk, fcntl.LOCK_EX)
            dados = ler(cfg["respostas"])
            h = dados["respostas"].setdefault(did, {"historico": []})["historico"]
            h.append(reg)
            del h[:-MAX_HISTORICO]
            dados["versao"] = 1
            gravar(cfg["respostas"], dados)
    return reg


class H(BaseHTTPRequestHandler):
    cfg = {}
    protocol_version = "HTTP/1.1"
    server_version = "painel-respostas"
    timeout = 15  # leitura de socket parada nao segura a thread para sempre

    def log_message(self, fmt, *a):
        pass

    def _json(self, codigo, obj):
        b = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(codigo)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(b)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(b)

    def _caminho(self):
        return self.path.split("?", 1)[0].rstrip("/")

    def do_GET(self):
        try:
            c = self._caminho()
            if c == "/api/saude":
                motivo = prontidao(self.cfg)
                if motivo:
                    return self._json(503, {"ok": False, "motivo": motivo})
                return self._json(200, {"ok": True})
            if c != "/api/respostas":
                raise Erro(404, "nao encontrado")
            self._json(200, ler(self.cfg["respostas"]))
        except Erro as e:
            self._json(e.codigo, {"erro": e.msg})

    def do_POST(self):
        try:
            if self._caminho() != "/api/respostas":
                raise Erro(404, "nao encontrado")
            o = self.headers.get("Origin")
            if o is not None and o not in self.cfg["origens"]:
                raise Erro(403, "origem nao permitida")
            ct = (self.headers.get("Content-Type") or "").split(";")[0].strip().lower()
            if ct != "application/json":
                raise Erro(415, "use Content-Type: application/json")
            try:
                n = int(self.headers.get("Content-Length", ""))
            except ValueError:
                raise Erro(411, "Content-Length obrigatorio")
            if n < 0 or n > self.cfg["max_corpo"]:
                self.close_connection = True
                raise Erro(413, "corpo grande demais")
            bruto = self.rfile.read(n)
            try:
                corpo = json.loads(bruto.decode("utf-8"))
            except ValueError:
                raise Erro(400, "JSON invalido")
            self._json(200, {"ok": True, "registro": registrar(self.cfg, corpo)})
        except Erro as e:
            self._json(e.codigo, {"erro": e.msg})
        except Exception:
            self._json(500, {"erro": "erro interno"})


def main():
    e = os.environ.get
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--data-json", default=e("PAINEL_DATA_JSON", "/data/html/data.json"))
    ap.add_argument("--respostas", default=e("PAINEL_RESPOSTAS", "/data/respostas/respostas.json"))
    ap.add_argument("--origens", default=e("PAINEL_ORIGENS", ORIGENS_PADRAO))
    ap.add_argument("--host", default=e("PAINEL_HOST", "0.0.0.0"))
    ap.add_argument("--porta", type=int, default=int(e("PAINEL_PORTA", "8000")))
    ap.add_argument("--max-corpo", type=int, default=MAX_CORPO)
    a = ap.parse_args()
    H.cfg = {"data_json": a.data_json, "respostas": a.respostas, "max_corpo": a.max_corpo,
             "origens": {x.strip() for x in a.origens.split(",") if x.strip()}}
    ThreadingHTTPServer.request_queue_size = 128  # padrao 5 perde conexoes em rajada
    srv = ThreadingHTTPServer((a.host, a.porta), H)
    srv.daemon_threads = True
    print("escutando %s:%d" % srv.server_address, flush=True)
    srv.serve_forever()


if __name__ == "__main__":
    main()
