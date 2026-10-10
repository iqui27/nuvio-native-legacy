#!/usr/bin/env bash
# Teste local da API de respostas e do mapa.py. Uso: tools/painel-issues/tests/api-respostas.sh
set -euo pipefail
AQUI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$AQUI/../../.." && pwd)"
T="$(mktemp -d)"; PID=""
trap '[ -n "$PID" ] && kill "$PID" 2>/dev/null || true; rm -rf "$T"' EXIT
FALHAS=0
ok()  { echo "PASS $1"; }
bad() { echo "FAIL $1"; FALHAS=$((FALHAS+1)); }
espera() { # nome esperado obtido
  if [ "$2" = "$3" ]; then ok "$1 (rc/http=$3)"; else bad "$1 (esperado $2, veio $3)"; fi
}

mkdir "$T/dados"
PORTA="$(python3 -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])')"
ORIGEM="http://192.168.1.20:8094"
cat > "$T/data.json" <<J
{"decisoes":[{"id":"dec-teste-um","pergunta":"?"},{"id":"dec-teste-dois","pergunta":"?"},{"id":"dec-teste-feita","pergunta":"?","aplicada_em":"2026-10-01"}]}
J
python3 "$AQUI/../api/servidor.py" --host 127.0.0.1 --porta "$PORTA" --data-json "$T/data.json" \
  --respostas "$T/dados/respostas.json" --origens "$ORIGEM,http://100.77.116.81:8094" >"$T/srv.log" 2>&1 &
PID=$!
for _ in $(seq 50); do curl -fs --max-time 2 "http://127.0.0.1:$PORTA/api/saude" >/dev/null 2>&1 && break; sleep 0.1; done
U="http://127.0.0.1:$PORTA/api/respostas"
post() { # corpo [headers extra...] -> http code
  local corpo="$1"; shift
  curl -s --max-time 10 -o "$T/out" -w '%{http_code}' -X POST -H 'Content-Type: application/json' "$@" --data "$corpo" "$U"
}

espera "POST valido" 200 "$(post '{"id":"dec-teste-um","resposta":"sim","nota":"ok"}' -H "Origin: $ORIGEM")"
python3 - "$T/dados/respostas.json" <<'P' && ok "gravado em respostas.json" || bad "gravado em respostas.json"
import json,sys
h=json.load(open(sys.argv[1]))["respostas"]["dec-teste-um"]["historico"]
assert h[-1]["resposta"]=="sim" and h[-1]["nota"]=="ok" and h[-1]["quando"]
P
espera "POST sem Origin (curl)" 200 "$(post '{"id":"dec-teste-um","resposta":"nao"}')"
espera "id inexistente" 404 "$(post '{"id":"dec-nao-existe","resposta":"sim"}')"
espera "id malformado" 400 "$(post '{"id":"../x","resposta":"sim"}')"
espera "resposta talvez" 400 "$(post '{"id":"dec-teste-um","resposta":"talvez"}')"
espera "nota 2000 chars" 200 "$(post "{\"id\":\"dec-teste-um\",\"resposta\":\"sim\",\"nota\":\"$(python3 -c 'print("a"*2000)')\"}")"
espera "nota 2001 chars" 400 "$(post "{\"id\":\"dec-teste-um\",\"resposta\":\"sim\",\"nota\":\"$(python3 -c 'print("a"*2001)')\"}")"
espera "corpo gigante" 413 "$(post "{\"id\":\"dec-teste-um\",\"resposta\":\"sim\",\"nota\":\"$(python3 -c 'print("a"*40000)')\"}")"
espera "Origin estranha" 403 "$(post '{"id":"dec-teste-um","resposta":"sim"}' -H 'Origin: http://evil.example')"
espera "decisao aplicada" 409 "$(post '{"id":"dec-teste-feita","resposta":"sim"}')"
espera "JSON invalido" 400 "$(post '{nao-json')"
espera "sem content-type json" 415 "$(curl -s --max-time 10 -o /dev/null -w '%{http_code}' -X POST -H 'Content-Type: text/plain' --data '{}' "$U")"

# prontidao
saude() { curl -s --max-time 5 -o "$T/s" -w '%{http_code}' "http://127.0.0.1:$PORTA/api/saude"; }
espera "saude pronta" 200 "$(saude)"; grep -Eq '"ok": true' "$T/s" && ok "saude diz ok" || bad "saude sem ok"
mv "$T/data.json" "$T/data.off"; espera "saude sem data.json" 503 "$(saude)"; grep -q motivo "$T/s" && ok "saude traz motivo" || bad "saude sem motivo"
mv "$T/data.off" "$T/data.json"
echo '{"decisoes":[]}' > "$T/vazio.json"; cp "$T/data.json" "$T/data.bak"; cp "$T/vazio.json" "$T/data.json"
espera "saude sem ids de decisao" 503 "$(saude)"; cp "$T/data.bak" "$T/data.json"
cp "$T/dados/respostas.json" "$T/resp.bak"; echo '{quebrado' > "$T/dados/respostas.json"
espera "saude com respostas.json corrompido" 503 "$(saude)"; cp "$T/resp.bak" "$T/dados/respostas.json"
espera "saude volta" 200 "$(saude)"

# concorrencia: 20 POSTs em paralelo
for i in $(seq 20); do
  ( post "{\"id\":\"dec-teste-dois\",\"resposta\":\"sim\",\"nota\":\"c$i\"}" -o /dev/null >"$T/c$i" ) &
done
LIMITE=$((SECONDS+30))
for i in $(seq 20); do
  while [ ! -s "$T/c$i" ]; do
    if ! kill -0 "$PID" 2>/dev/null || [ "$SECONDS" -ge "$LIMITE" ]; then bad "espera dos POSTs concorrentes (servidor morreu ou passou de 30s)"; break; fi
    sleep 0.05
  done
done
[ "$(cat "$T"/c[0-9]* | tr -d '\n')" = "$(printf '200%.0s' $(seq 20))" ] && ok "20 POSTs concorrentes todos 200" || bad "algum POST concorrente nao deu 200"
python3 - "$T/dados/respostas.json" <<'P' && ok "20 escritas concorrentes, JSON valido e completo" || bad "concorrencia"
import json,sys
d=json.load(open(sys.argv[1]))
notas=sorted(x["nota"] for x in d["respostas"]["dec-teste-dois"]["historico"])
assert notas==sorted("c%d"%i for i in range(1,21)), notas
assert len(d["respostas"]["dec-teste-um"]["historico"])==3
P
ls "$T" | grep -q '\.tmp$' && bad "sobrou arquivo tmp" || ok "sem arquivo tmp sobrando"
curl -s --max-time 10 "$U" | python3 -c 'import json,sys;d=json.load(sys.stdin);assert len(d["respostas"]["dec-teste-dois"]["historico"])==20' \
  && ok "GET devolve tudo" || bad "GET"

# mapa.py: copia isolada com um respostas-dono.json de exemplo
M="$T/mapa"; mkdir "$M"; cp "$REPO/docs/issues/"{mapa.py,mapa.json,MAPA.md} "$M/"
rc=0; python3 "$M/mapa.py" --check >/dev/null 2>&1 || rc=$?; espera "mapa.py --check com ids" 0 "$rc"
ID="$(python3 -c "import json;print(json.load(open('$M/mapa.json'))['decisoes'][0]['id'])")"
cat > "$M/respostas-dono.json" <<J
{"versao":1,"respostas":{"$ID":{"historico":[{"resposta":"nao","nota":"so depois da 2.0.5","quando":"2026-10-09T12:00:00-03:00"}]}}}
J
python3 "$M/mapa.py" >/dev/null && grep -q "Resposta do dono: NÃO\*\* Nota: so depois da 2.0.5" "$M/MAPA.md" && grep -q "respondida, a aplicar" "$M/MAPA.md" \
  && ok "mapa.py mostra a resposta" || bad "mapa.py mostra a resposta"
# nota do dono e entrada nao confiavel: sem HTML nem link/imagem no MAPA.md
python3 - "$M/respostas-dono.json" "$ID" <<'P'
import json,sys
nota='<img src=x onerror=alert(1)> [x](javascript:alert(1)) ![i](http://a/b.png) <http://evil>'
json.dump({"versao":1,"respostas":{sys.argv[2]:{"historico":[{"resposta":"sim","nota":nota,"quando":"2026-10-09T12:00:00-03:00"}]}}},open(sys.argv[1],"w"))
P
python3 "$M/mapa.py" >/dev/null
LN="$(grep -F "$ID" "$M/MAPA.md" | head -1)"
case "$LN" in *"<img"*|*"](javascript"*|*"](http"*|*"<http"*) bad "nota vazou HTML/link no MAPA.md: $LN";; *) ok "nota escapada (sem <img, sem link markdown)";; esac
case "$LN" in *"&lt;img"*) ok "HTML virou texto (&lt;)";; *) bad "esperava &lt;img";; esac

# respostas.sh cai para o caminho antigo quando o novo nao existe (ssh falso)
mkdir "$T/fakebin"; cat > "$T/fakebin/ssh" <<'S'
#!/usr/bin/env bash
for a in "$@"; do case "$a" in */respostas/respostas.json) exit 1;; */respostas.json) cat "$ANTIGO_FAKE"; exit 0;; esac; done; exit 1
S
chmod +x "$T/fakebin/ssh"; echo '{"versao":1,"respostas":{"dec-velho":{"historico":[]}}}' > "$T/antigo.json"
RS2="$T/repo2"; mkdir -p "$RS2/docs/issues"
rc=0; PATH="$T/fakebin:$PATH" ANTIGO_FAKE="$T/antigo.json" "$AQUI/../respostas.sh" baixar "$RS2" >/dev/null || rc=$?
espera "respostas.sh cai para o caminho antigo" 0 "$rc"; grep -q dec-velho "$RS2/docs/issues/respostas-dono.json" && ok "conteudo do caminho antigo" || bad "conteudo antigo"

# id que some sem ir para decisoes_ids_retirados => erro (compara com o HEAD de um git temporario)
cp "$REPO/docs/issues/mapa.json" "$M/mapa.json"; rm -f "$M/respostas-dono.json"
git -C "$M" init -q && git -C "$M" add mapa.json && git -C "$M" -c user.name=t -c user.email=t@t commit -qm base 2>/dev/null
# HEAD:./mapa.json precisa existir no repo temporario: mapa.py usa o git do proprio diretorio
python3 - "$M/mapa.json" <<'P'
import sys
p=sys.argv[1]; t=open(p).read(); t=t.replace('"id": "dec-135-vidaa-rtl",','"id": "dec-135-trocado",',1); open(p,'w').write(t)
P
rc=0; python3 "$M/mapa.py" >/dev/null 2>"$T/err" || rc=$?; espera "id sumido sem retirar falha" 1 "$rc"
grep -q "sumiu" "$T/err" && ok "mensagem diz que o id sumiu" || bad "mensagem do id sumido"
python3 - "$M/mapa.json" <<'P'
import sys
p=sys.argv[1]; t=open(p).read()
t=t.replace('"decisoes_ids_retirados": [],','"decisoes_ids_retirados": ["dec-135-vidaa-rtl"],',1); open(p,'w').write(t)
P
rc=0; python3 "$M/mapa.py" >/dev/null 2>&1 || rc=$?; espera "id sumido mas retirado passa" 0 "$rc"
python3 - "$M/mapa.json" <<'P'
import sys
p=sys.argv[1]; t=open(p).read()
t=t.replace('"id": "dec-135-trocado",','"id": "dec-135-vidaa-rtl",',1); open(p,'w').write(t)
P
rc=0; python3 "$M/mapa.py" >/dev/null 2>&1 || rc=$?; espera "id retirado reusado falha" 1 "$rc"
# retirado no HEAD, tirado da lista e re-adicionado a decisoes: tem de falhar
cp "$REPO/docs/issues/mapa.json" "$M/mapa.json"
python3 - "$M/mapa.json" <<'P'
import sys
p=sys.argv[1]; t=open(p).read()
t=t.replace('"decisoes_ids_retirados": [],','"decisoes_ids_retirados": ["dec-antigo-x"],',1); open(p,'w').write(t)
P
git -C "$M" add mapa.json && git -C "$M" -c user.name=t -c user.email=t@t commit -qm retirado
python3 - "$M/mapa.json" <<'P'
import sys
p=sys.argv[1]; t=open(p).read()
t=t.replace('"decisoes_ids_retirados": ["dec-antigo-x"],','"decisoes_ids_retirados": [],',1)
t=t.replace('"id": "dec-135-vidaa-rtl",','"id": "dec-antigo-x",',1); open(p,'w').write(t)
P
rc=0; python3 "$M/mapa.py" >/dev/null 2>"$T/err" || rc=$?; espera "retirado no HEAD volta a ativo falha" 1 "$rc"
grep -q "voltou a ficar ativo" "$T/err" && ok "mensagem do retorno de id retirado" || bad "mensagem do retorno"

# respostas.sh baixar: escrita atomica, sem truncar em caso de JSON invalido
RS="$T/repo"; mkdir -p "$RS/docs/issues"; echo '{"versao":1,"respostas":{"dec-a":{"historico":[]}}}' > "$RS/docs/issues/respostas-dono.json"
echo '{lixo' > "$T/ruim.json"
rc=0; PAINEL_RESPOSTAS_ARQUIVO="$T/ruim.json" "$AQUI/../respostas.sh" baixar "$RS" >/dev/null 2>&1 || rc=$?
[ "$rc" != 0 ] && grep -q dec-a "$RS/docs/issues/respostas-dono.json" && ok "respostas.sh nao trunca o destino com origem invalida" || bad "respostas.sh truncou"
rc=0; PAINEL_RESPOSTAS_ARQUIVO="$T/dados/respostas.json" "$AQUI/../respostas.sh" baixar "$RS" >/dev/null || rc=$?
espera "respostas.sh baixar valido" 0 "$rc"; ls -A "$RS/docs/issues" | grep -q '\.tmp$' && bad "tmp sobrando em respostas.sh" || ok "respostas.sh sem tmp"
# painel no navegador: polling de 30 s, rascunho e restauracao de scroll (DOM e fetch falsos)
if command -v node >/dev/null 2>&1; then
  rc=0; node "$AQUI/painel-js.mjs" || rc=$?
  espera "index.html: polling, rascunho e scroll (painel-js.mjs)" 0 "$rc"
else
  echo "PULADO index.html: node nao instalado (tests/painel-js.mjs)"
fi

[ "$FALHAS" = 0 ] && { echo "TUDO OK"; exit 0; } || { echo "$FALHAS falha(s)"; exit 1; }
