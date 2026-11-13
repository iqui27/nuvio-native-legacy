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
# pedido que CHEGOU ao fixture com caminho desconhecido (ou Host estranho)
# falha aqui. Limite: o log so ve o que chega ao fixture; nao prova ausencia
# de pedido para outro host. O fechamento vem da fonte: a unica URL de
# servidor compilada e a do fixture, demais -D sao sinteticos ou vazios.
#
# Isolamento: os binarios rodam com HOME, XDG_* e NUVIO_DADOS dentro de $TMP
# (o fallback de src/dados.c para $HOME/.nuvio nunca alcanca a pasta do dono)
# e sem proxy herdado (pedidos ao loopback nao passam por proxy do chamador).
set -e
cd "$(dirname "$0")/.."
FONTES=$(ls src/*.c src/dts/*.c | grep -v 'src/main.c' | tr '\n' ' ')
TMP=$(mktemp -d "$TMPDIR/nuvio-conta.XXXXXX")
FIX_PID=""
cleanup() { [ -n "$FIX_PID" ] && kill "$FIX_PID" 2>/dev/null; rm -rf "$TMP"; }
trap cleanup EXIT
unset http_proxy https_proxy HTTP_PROXY HTTPS_PROXY all_proxy ALL_PROXY no_proxy NO_PROXY
mkdir -p "$TMP/home" "$TMP/xdg/config" "$TMP/xdg/cache" "$TMP/xdg/data" "$TMP/xdg/state" "$TMP/xdg/run"
chmod 700 "$TMP/xdg/run"
ISO=(HOME="$TMP/home" XDG_CONFIG_HOME="$TMP/xdg/config" XDG_CACHE_HOME="$TMP/xdg/cache"
     XDG_DATA_HOME="$TMP/xdg/data" XDG_STATE_HOME="$TMP/xdg/state" XDG_RUNTIME_DIR="$TMP/xdg/run")

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
env "${ISO[@]}" NUVIO_DADOS="$TMP/dados" "$TMP/logout"

echo
echo "==> pedidos que chegaram ao fixture: rota conhecida, Host loopback (nao prova ausencia de egress)"
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
    for r in viol + fora: print("  VIOLACAO: %s %s host=%s" % (r["metodo"], r["caminho"], r["host"]))
    print("FALHOU: %d rota(s) desconhecida(s), %d Host nao-loopback no log do fixture" % (len(viol), len(fora)))
    sys.exit(1)
print("  %d pedido(s) chegaram ao fixture: rotas conhecidas, Host loopback" % total)
PY

echo
echo "==> ajustes da conta chegam com o sentido certo"
cc tests/conta_ajustes.c $FONTES -o "$TMP/ajustes" "${ENV_D[@]}" -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz \
  -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
env "${ISO[@]}" "$TMP/ajustes" || RC_AJUSTES=$?

# Segunda conferencia: o binario de ajustes nao devia ter tocado na rede — se
# tocou, aparece aqui. O rc do teste e repassado no final do script.
echo
echo "==> depois do teste de ajustes, nenhum pedido novo a rota desconhecida do fixture"
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
    for r in viol + fora: print("  VIOLACAO: %s %s host=%s" % (r["metodo"], r["caminho"], r["host"]))
    print("FALHOU: %d rota(s) desconhecida(s), %d Host nao-loopback no log do fixture" % (len(viol), len(fora)))
    sys.exit(1)
print("  %d pedido(s) chegaram ao fixture: rotas conhecidas, Host loopback" % total)
PY

echo
echo "==> QR legivel por leitor de verdade"
cc tools/qr_despejo.c src/qr.c -o "$TMP/qr" -Isrc
PY=${NUVIO_PY:-${TMPDIR:-/tmp}/qrvenv/bin/python}
if [ -x "$PY" ]; then "$PY" tools/qr_conferir.py "$TMP/qr"
else echo "   PULADO: sem venv com opencv (ver o cabecalho de tools/qr_conferir.py)"; fi

exit ${RC_AJUSTES:-0}
