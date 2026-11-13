#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Testes da conta, OFFLINE: o servidor e um fixture Supabase sintetico e
# descartavel em 127.0.0.1 (tests/nuvem_fixture.py). Nenhuma configuracao do
# dono e lida (env.sh/local.properties NAO entram aqui): as -D sao passadas
# literalmente, com a URL do fixture. O transporte real (rede.c) e o ciclo
# nuvem/sessao/sync rodam inteiros — so que contra o loopback.
#
#   bash tests/conta.sh
#
# O de ajustes nao precisa de rede. O de logout abre uma sessao ANONIMA no
# fixture e a grava como se fosse de usuario. Nao e gambiarra de teste: o
# usuario anonimo tem conta real no servidor (o proprio app web faz isso antes
# de pedir o codigo de login), com addons semeados — o fixture reproduz essa
# forma com dados de mentira. Depois do binario, o log do fixture e conferido:
# pedido para host que nao seja 127.0.0.1, ou caminho desconhecido, falha aqui.
set -e
cd "$(dirname "$0")/.."
FONTES=$(ls src/*.c src/dts/*.c | grep -v 'src/main.c' | tr '\n' ' ')
TMP=$(mktemp -d)
FIX_PID=""
cleanup() { [ -n "$FIX_PID" ] && kill "$FIX_PID" 2>/dev/null; rm -rf "$TMP"; }
trap cleanup EXIT

echo "==> subindo fixture de conta em 127.0.0.1"
python3 tests/nuvem_fixture.py --port 0 --state "$TMP/fix" &
FIX_PID=$!
for i in $(seq 1 50); do [ -s "$TMP/fix/port.json" ] && break; sleep 0.1; done
PORT=$(python3 -c "import json;print(json.load(open('$TMP/fix/port.json'))['port'])")
NUVIO_FIX_URL="http://127.0.0.1:$PORT"
VER=$(sed -n 's/^[[:space:]]*"version":[[:space:]]*"\([^"]*\)".*/\1/p' deploy/app/appinfo.json | head -1)

# -D sinteticos: mesmos nomes de tools/env.sh, valores de mentira. So a URL do
# servidor e real — e aponta para o fixture.
ENV_D=(
  "-DNV_SUPABASE_URL=\"$NUVIO_FIX_URL\""
  "-DNV_SUPABASE_ANON_KEY=\"fixture-anon-key\""
  "-DNV_TV_LOGIN_BASE=\"$NUVIO_FIX_URL/login\""
  "-DNV_TRAKT_CLIENT_ID=\"fixture-trakt-id\""
  "-DNV_TRAKT_CLIENT_SECRET=\"fixture-trakt-secret\""
  "-DNV_SIMKL_CLIENT_ID=\"fixture-simkl-id\""
  "-DNV_SIMKL_APP=\"fixture-simkl-app\""
  "-DNV_TMDB_API_KEY=\"fixture-tmdb-key\""
  "-DNV_SEEKR_API_KEY=\"\""
  "-DNV_REC_URL=\"\""
  "-DNV_LOGLINK_TOKEN=\"\""
  "-DNV_DISCORD_CLIENT_ID=\"\""
  "-DNV_VERSAO=\"$VER\""
)

echo "==> logout apaga o usuario anterior"
cc tests/conta_logout.c $FONTES -o "$TMP/logout" "${ENV_D[@]}" -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz \
  -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$TMP/dados" "$TMP/logout"

echo
echo "==> todo pedido saiu para o fixture (host 127.0.0.1, caminho conhecido)"
python3 - "$TMP/fix/log.jsonl" <<'PY'
import json, sys
viol, fora, total = [], [], 0
for linha in open(sys.argv[1]):
    rec = json.loads(linha); total += 1
    if rec.get("violacao"): viol.append(rec)
    host = rec.get("host", "")
    if not (host.startswith("127.0.0.1") or host.startswith("localhost")):
        fora.append(rec)
if viol or fora:
    for r in viol + fora: print("  FORA: %s %s host=%s" % (r["metodo"], r["caminho"], r["host"]))
    print("FALHOU: %d violacao(oes), %d pedido(s) fora do loopback" % (len(viol), len(fora)))
    sys.exit(1)
print("  %d pedido(s), todos em 127.0.0.1 e em rotas conhecidas" % total)
PY

echo
echo "==> ajustes da conta chegam com o sentido certo"
cc tests/conta_ajustes.c $FONTES -o "$TMP/ajustes" "${ENV_D[@]}" -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz \
  -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$TMP/dados-ajustes" "$TMP/ajustes" || RC_AJUSTES=$?

# Segunda conferencia: o binario de ajustes nao devia ter tocado na rede — se
# tocou, aparece aqui. O rc do teste e repassado no final do script.
echo
echo "==> depois do teste de ajustes, o fixture continua limpo"
python3 - "$TMP/fix/log.jsonl" <<'PY'
import json, sys
viol, fora, total = [], [], 0
for linha in open(sys.argv[1]):
    rec = json.loads(linha); total += 1
    if rec.get("violacao"): viol.append(rec)
    host = rec.get("host", "")
    if not (host.startswith("127.0.0.1") or host.startswith("localhost")):
        fora.append(rec)
if viol or fora:
    for r in viol + fora: print("  FORA: %s %s host=%s" % (r["metodo"], r["caminho"], r["host"]))
    print("FALHOU: %d violacao(oes), %d pedido(s) fora do loopback" % (len(viol), len(fora)))
    sys.exit(1)
print("  %d pedido(s), todos em 127.0.0.1 e em rotas conhecidas" % total)
PY

echo
echo "==> QR legivel por leitor de verdade"
cc tools/qr_despejo.c src/qr.c -o "$TMP/qr" -Isrc
PY=${NUVIO_PY:-${TMPDIR:-/tmp}/qrvenv/bin/python}
if [ -x "$PY" ]; then "$PY" tools/qr_conferir.py "$TMP/qr"
else echo "   PULADO: sem venv com opencv (ver o cabecalho de tools/qr_conferir.py)"; fi

exit ${RC_AJUSTES:-0}
