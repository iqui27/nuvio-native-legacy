#!/bin/bash
# Contrato do cofre do diario contra o Worker LOCAL (wrangler dev --local).
#   bash servidor/recomendacoes/preparar-local.sh
#   npx wrangler@4 dev --local --port 8799 --config servidor/recomendacoes/wrangler.toml &
#   DIARIO_URL=http://127.0.0.1:8799 bash tests/diarionuvem_servidor.sh
set -eu
cd "$(dirname "$0")/.."
BASE="${DIARIO_URL:-http://127.0.0.1:8799}"
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
