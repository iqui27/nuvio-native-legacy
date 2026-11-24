#!/bin/bash
# Retomada sem spoiler na P6 (lote sense, issue 57): P6 real + compositor real
# contra um servidor local que grava cada corpo HTTP (tests/farol_retomada.c).
#
#   TMPDIR=/caminho SANITIZE=0 bash tests/farol_retomada.sh   (normal)
#   TMPDIR=/caminho SANITIZE=1 bash tests/farol_retomada.sh   (ASan+UBSan, padrao)
set -eu
cd "$(dirname "$0")/.."

if [ -z "${TMPDIR:-}" ]; then
  echo "farol_retomada: TMPDIR obrigatorio (sem fallback para /tmp)" >&2
  exit 2
fi
TMPDIR="${TMPDIR%/}"
mkdir -p "$TMPDIR"

san="${SANITIZE:-1}"
flags=(-Wall -Wextra -Wno-unused-function -O1 -g -std=gnu11 -Isrc -Itests -pthread)
case "$san" in
  0) ;;
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  *) echo "farol_retomada: SANITIZE desconhecido: $san" >&2; exit 2 ;;
esac
if [ "$(uname -s)" = "Darwin" ]; then
  sdl_cflags=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
else
  read -r -a sdl_cflags <<< "$(pkg-config --cflags sdl2 2>/dev/null || true)"
  flags+=(-ldl)
fi

work=$(mktemp -d "$TMPDIR/nuvio-farol-retomada.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$work"' EXIT

cat > "$work/srv.py" <<'PY'
import http.server, json, os, sys, time

porta_arq, cen_arq, cap_arq = sys.argv[1:4]

def cena(x, a=("2", "4"), alvo="c0"):
    return {"v": 1, "tela": "detalhe", "origem": "llm",
            "pecas": [{"t": "info", "alvo": alvo, "itens": [{"k": "retomada",
                       "l": {"m": "FT_INFO_RETOMADA_SIMPLES", "a": list(a), "x": x}}]}]}

OK = "Walt e Jesse ficaram presos no deserto."
RESP = {
    "ok":         (200, cena(OK)),
    "lento":      (200, cena(OK)),
    "s500":       (500, {"erro": "x"}),
    "s429":       (429, {"erro": "limite diario"}),
    "regras":     (200, {"origem": "regras"}),
    "nl":         (200, cena("Walt e Jesse no deserto.\nE depois fogem.")),
    "proibida":   (200, cena("Olá! Walt e Jesse no deserto.")),
    "t2e5":       (200, cena(OK, a=("2", "5"))),
    "alvo_outro": (200, cena(OK, alvo="tt0000001")),
    "longa":      (200, cena("Walt e Jesse ficaram presos no deserto com o trailer sem bateria.")),
    "ep5":        (200, cena("No episódio 5, Walt volta ao deserto.")),
    "ep5_en":     (200, cena("In episode 5, Walt goes home.")),
    "temp3":      (200, cena("Na terceira temporada, Walt muda de identidade.")),
    "futuro":     (200, cena("Walt morrerá ao salvar Jesse.")),
}

class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass
    def do_POST(self):
        n = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(n)
        with open(cap_arq, "ab") as f:
            f.write(raw.replace(b"\n", b" ") + b"\n")
        try:
            cen = open(cen_arq).read().strip()
        except OSError:
            cen = "ok"
        if self.path != "/v1/farol/compor":
            st, corpo = 404, b"{}"
        elif cen == "torto":
            st, corpo = 200, b'{"v":1,"tela":"detalhe","pecas":['
        else:
            st, doc = RESP.get(cen, (500, {}))
            corpo = json.dumps(doc, ensure_ascii=False).encode()
        if cen == "lento":
            time.sleep(8)
        try:
            self.send_response(st)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(corpo)))
            self.end_headers()
            self.wfile.write(corpo)
        except OSError:
            pass

s = http.server.ThreadingHTTPServer(("127.0.0.1", 0), H)
s.daemon_threads = True
with open(porta_arq + ".tmp", "w") as f:
    f.write(str(s.server_address[1]))
os.replace(porta_arq + ".tmp", porta_arq)
s.serve_forever()
PY

: > "$work/captura"
echo ok > "$work/cenario"
python3 "$work/srv.py" "$work/porta" "$work/cenario" "$work/captura" &
srv=$!
for _ in $(seq 50); do [ -s "$work/porta" ] && break; sleep 0.1; done
test -s "$work/porta"
base="http://127.0.0.1:$(cat "$work/porta")"

"${CC:-cc}" "${flags[@]}" "${sdl_cflags[@]}" \
  tests/farol_retomada.c \
  src/faroldetalhe.c src/farolcena.c src/farolrec.c src/farolgosto.c \
  src/farolinfo.c src/farolpeca.c src/faroltexto.c src/farolficha.c \
  src/farolcompor.c src/rede.c src/redeurl.c \
  src/js.c src/idioma.c src/linguas.c \
  -lm -o "$work/retomada"

"$work/retomada" "$base" "$work/cenario" "$work/captura"

# Auditoria final de todos os corpos: so a sinopse vista, zero canario/token.
python3 - "$work/captura" <<'PY'
import json, sys
linhas = [l for l in open(sys.argv[1], encoding="utf-8").read().splitlines() if l.strip()]
assert linhas, "nenhum POST capturado"
for l in linhas:
    for proibido in ("CanarioProximoEpisodio57", "CanarioProximaTemporada57",
                     "SinopseAnteriorT2E3", "tok-retomada-57"):
        assert proibido not in l, f"vazou {proibido}: {l[:200]}"
    d = json.loads(l)
    assert d["modo"] == "retomada", d["modo"]
    r = d["retomada"]
    assert set(r) == {"temporada", "episodio", "sinopse"}, r
    assert (r["temporada"], r["episodio"]) == (2, 4), r
    assert r["sinopse"].startswith("SinopseVistaT2E4"), r
    assert [c["id"] for c in d["candidatos"]] == ["tt0903747"], d["candidatos"]
print(f"farol_retomada: {len(linhas)} corpos auditados, so T2E4 visto")
PY

# Contratos: farolinfo continua puro; faroldetalhe sem alocacao nem log.
if grep -nE '(malloc|calloc|realloc|free)[[:space:]]*\(' src/faroldetalhe.c src/farolinfo.c; then
  echo "farol_retomada: FALHA (alocacao dinamica)" >&2; exit 1
fi
if grep -nE '\b(printf|fprintf|puts|syslog|__android_log)[[:space:]]*\(' src/faroldetalhe.c src/farolinfo.c; then
  echo "farol_retomada: FALHA (log)" >&2; exit 1
fi
echo "farol_retomada: tudo ok (SANITIZE=$san)"
