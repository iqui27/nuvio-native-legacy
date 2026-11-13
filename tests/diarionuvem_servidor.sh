#!/bin/bash
# Contrato do cofre do diario contra o Worker LOCAL (wrangler dev --local).
# Sem DIARIO_URL o teste SOBE o proprio servidor numa porta loopback aleatoria,
# com D1 temporario, HOME/XDG isolados e env file sintetico — nada de
# ~/.wrangler, .dev.vars ou .wrangler/state do projeto (ver
# tests/servidor_local.sh). Com DIARIO_URL mantem o comportamento antigo e
# bate no servidor ja no ar. Sem wrangler instalado: FALHA, nao pula.
set -eu
cd "$(dirname "$0")/.."
. tests/servidor_local.sh
work=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-diario-srv.XXXXXX)
DEV=""
if [ -n "${DIARIO_URL:-}" ]; then
  BASE="$DIARIO_URL"
else
  NV_W="$(nv_wrangler_bin)" || { echo "FALHOU: wrangler 4.x nao instalado"; rm -rf "$work"; exit 1; }
  nv_isolate_env "$work"
  nv_rec_test_config "$work/wrangler.toml"
  DEVINFO="$(nv_dev_start "$work/wrangler.toml" "$work")" || exit 1
  PORT=${DEVINFO%% *}; DEV=${DEVINFO##* }
  BASE="http://127.0.0.1:$PORT"
  NUVIO_WRANGLER="$NV_W" NUVIO_D1_CFG="$work/wrangler.toml" \
    NUVIO_D1_PERSIST="$work/persist" NUVIO_D1_LOG="$work/d1.log" \
    bash servidor/recomendacoes/preparar-local.sh > "$work/prep.log" 2>&1 \
    || { tail -20 "$work/prep.log"; nv_dev_stop "$DEV"; rm -rf "$work"; exit 1; }
fi
trap 'nv_dev_stop "$DEV"; rm -rf "$work"' EXIT
out="${TMPDIR:-/tmp}/nuvio-diarionuvem-servidor"
cc src/diarionuvem.c src/diario.c src/diarionomes.c src/js.c src/jsw.c tests/diarionuvem_servidor.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -o "$out" -O1 -g -Wall \
  -Wno-deprecated-declarations -DNV_REC_URL='""'
H=(-H "authorization: Bearer tok-a" -H "x-nuvio-auth: nuvio" -H "x-nuvio-perfil: 3" -H "content-type: application/json")
curl -s -X POST "${H[@]}" -d '{"ligado":0}' "$BASE/v1/diario/cofre" > /dev/null
c=$(curl -s -o /dev/null -w '%{http_code}' -X POST "${H[@]}" --data-binary "$("$out" lote)" "$BASE/v1/diario/lote")
[ "$c" = 409 ] || { echo "FALHOU: sem consentimento devia ser 409, veio $c"; exit 1; }
curl -s -X POST "${H[@]}" -d '{"ligado":1}' "$BASE/v1/diario/cofre" > /dev/null
r=$(curl -s -X POST "${H[@]}" --data-binary "$("$out" lote)" "$BASE/v1/diario/lote")
echo "$r" | grep -q '"gravadas":3' || { echo "FALHOU: lote: $r"; exit 1; }
curl -s "${H[@]}" "$BASE/v1/diario?desde=0" | "$out" ler
curl -s -X POST "${H[@]}" -d '{"ligado":0}' "$BASE/v1/diario/cofre" > /dev/null
n=$(curl -s "${H[@]}" "$BASE/v1/diario?desde=0" | grep -o '"chave"' | wc -l | tr -d ' ')
[ "$n" = 0 ] || { echo "FALHOU: desligar devia apagar ($n)"; exit 1; }
echo "diarionuvem_servidor: desligar apagou: ok"
