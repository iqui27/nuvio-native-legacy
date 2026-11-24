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
    "p1_ep_cinco_pt":  (200, cena("No episódio cinco, Walt volta ao deserto.")),
    "p1_ep_five_en":   (200, cena("In episode five, Walt returns to the desert.")),
    "p1_ep_cinco_es":  (200, cena("En el episodio cinco, Walt vuelve al desierto.")),
    "p1_eleventh_en":  (200, cena("In the eleventh episode, Walt returns to the desert.")),
    "p1_3ponto_a":     (200, cena("Walt vuelve al desierto en la 3.ª temporada.")),
    "p1_chegara":      (200, cena("Walt chegará ao deserto.")),
    "will_die":        (200, cena("Walt will die in the desert.")),
    "he_will":         (200, cena("He will return.")),
    "ep_21_pt":        (200, cena("No episódio vinte e um, Walt foge.")),
    "ep_21_en":        (200, cena("In the twenty-first episode, Walt escapes.")),
    "ep_21_es":        (200, cena("En el episodio veintiuno, Walt huye.")),
    "pos_revelara":    (200, cena("Jesse soube que Walt revelara o segredo no deserto.")),
    "pos_escapara":    (200, cena("Jesse negó que Walt escapara del desierto.")),
    "pos_will_found":  (200, cena("Will found a trail in the desert.")),
    "pos_proximo_ao":  (200, cena("Walt encontrou Jesse próximo ao deserto.")),
    "ctrl_ep4":         (200, cena("No episódio 4, Walter confronta Jesse no deserto.")),
    "ctrl_temp2":       (200, cena("Na 2ª temporada, Walter encontrou Jesse no deserto.")),
    "p1_r3_ep_no5":     (200, cena("In episode no. 5, Walt dies.")),
    "p1_r3_ep_n5":      (200, cena("No episódio n. 5, Walt morre.")),
    "p1_r3_seas_no3":   (200, cena("In season no. 3, Walt dies.")),
    "p1_r3_ep_ponto5":  (200, cena("No ep. 5, Walt morre.")),
    "p1_r3_cap_ponto5": (200, cena("No cap. 5, Walt morre.")),
    "p1_r3_temp_ponto3": (200, cena("Na temp. 3, Walt morre.")),
    "p1_r3_ep_nro5":    (200, cena("In episode nro. 5, Walt dies.")),
    "ctrl_ep_no4":      (200, cena("In episode no. 4, Walt confronts Jesse.")),
    "ctrl_ep_n4":       (200, cena("No episódio n. 4, Walt confronta Jesse.")),
    "pos_alvara":       (200, cena("Walt perde o alvará do laboratório.")),
    "pos_guarana_para": (200, cena("Walt toma guaraná com maracujá no Pará.")),
    "pos_ep_ponto_fim": (200, cena("Walt watched the episode. 5 days later he ran.")),
    "ep_21_en_ok":         (200, cena("In the twenty-first episode, Walt flees.", a=("2", "21"))),
    "ep_11_pt_ok":         (200, cena("No décimo primeiro episódio, Walt foge.", a=("2", "11"))),
    "p1_r4_21_later":      (200, cena("Twenty-one episodes later, Walt dies.", a=("2", "21"))),
    "p1_r4_22_depois":     (200, cena("Vinte e dois episódios depois, Walt morre.", a=("2", "22"))),
    "p1_r4_4_later":       (200, cena("Four episodes later, Walt dies.")),
    "p1_r4_3temp_despues": (200, cena("Tres temporadas después, Walt muere.", a=("3", "4"))),
    "p1_r5_4_from_now":    (200, cena("Four episodes from now, Walt dies.")),
    "p1_r5_21_from_now":   (200, cena("Twenty-one episodes from now, Walt dies.", a=("2", "21"))),
    "p1_r5_21_pass":       (200, cena("Twenty-one episodes pass before Walt dies.", a=("2", "21"))),
    "p1_r5_22_se_passam":  (200, cena("Vinte e dois episódios se passam até Walt morrer.", a=("2", "22"))),
    "p1_r5_eps_4_5":       (200, cena("In episodes 4 and 5, Walt dies.")),
    "p1_r5_one_ep_later":  (200, cena("One episode later, Walt dies.", a=("2", "1"))),
    "p1_r5_1_ep_from_now": (200, cena("1 episode from now, Walt dies.", a=("2", "1"))),
    "p1_r5_an_ep_later":   (200, cena("An episode later, Walt dies.", a=("2", "1"))),
    "p1_r5_um_ep_depois":  (200, cena("Um episódio depois, Walt morre.", a=("2", "1"))),
    "p1_r5_one_seas_later":(200, cena("One season later, Walt dies.", a=("1", "4"))),
    "pos_ep_ends_later":     (200, cena("The episode ends. Later Walt runs.")),
    "pos_ep_virg_depois":    (200, cena("No episódio, depois Walt foge.")),
    "p1_r6_several_later":   (200, cena("Several episodes later, Walt dies.")),
    "p1_r6_alguns_depois":   (200, cena("Alguns episódios depois, Walt morre.")),
    "p1_r6_varias_despues":  (200, cena("Varias temporadas después, Walt muere.")),
    "p1_r6_ep_after_this":   (200, cena("The episode after this one ends with Walt's death.")),
    "p1_r6_ep_depois_deste": (200, cena("O episódio depois deste termina com a morte de Walt.")),
    "p1_r6_ep_despues_este": (200, cena("El episodio después de este termina con la muerte de Walt.")),
    "p1_r7_ep_after_current": (200, cena("The episode after the current one ends with Walt's death.")),
    "p1_r7_ep_depois_atual":  (200, cena("O episódio depois do atual termina com a morte de Walt.")),
    "p1_r7_ep_despues_actual":(200, cena("El episodio después del actual acaba con la muerte de Walt.")),
    "p1_r7_ep_after_saw":     (200, cena("The episode after the one Walt saw ends with his death.")),
    "p1_r7_couple_eps_pass":  (200, cena("A couple of episodes pass before Walt dies.")),
    "p1_r6_several_pass":    (200, cena("Several episodes pass before Walt dies.")),
    "p1_r6_episodes_later":  (200, cena("Episodes later, Walt dies.")),
    "p1_r6_seasons_from_now":(200, cena("Seasons from now, Walt dies.")),
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
    assert (r["temporada"], r["episodio"]) in ((1, 4), (2, 1), (2, 4), (2, 11), (2, 21), (2, 22), (3, 4)), r
    assert r["sinopse"].startswith(f"SinopseVistaT{r['temporada']}E{r['episodio']}"), r
    assert [c["id"] for c in d["candidatos"]] == ["tt0903747"], d["candidatos"]
print(f"farol_retomada: {len(linhas)} corpos auditados, so episodio visto")
PY

# Contratos: farolinfo continua puro; faroldetalhe sem alocacao nem log.
if grep -nE '(malloc|calloc|realloc|free)[[:space:]]*\(' src/faroldetalhe.c src/farolinfo.c; then
  echo "farol_retomada: FALHA (alocacao dinamica)" >&2; exit 1
fi
if grep -nE '\b(printf|fprintf|puts|syslog|__android_log)[[:space:]]*\(' src/faroldetalhe.c src/farolinfo.c; then
  echo "farol_retomada: FALHA (log)" >&2; exit 1
fi
echo "farol_retomada: tudo ok (SANITIZE=$san)"
