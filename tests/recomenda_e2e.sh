#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# O cliente C contra o Worker de verdade, em LOCAL.
#
# MODO PADRAO (sem argumento): o teste SOBE o proprio `wrangler dev` numa porta
# loopback aleatoria com D1/KV temporarios (--persist-to em mktemp), HOME/XDG
# isolados e um .env sintetico vazio — nada de ~/.wrangler, .dev.vars ou do
# .wrangler/state do projeto. Wrangler vem de NUVIO_WRANGLER, node_modules ou
# do cache ja instalado (tests/servidor_local.sh), nunca de `npx wrangler@4`.
# Sem wrangler instalado o teste FALHA (nao pula): a suite global depende dele.
#
# MODO SERVIDOR EXTERNO (com argumento): `bash tests/recomenda_e2e.sh URL`
# preserva o contrato antigo — se a URL nao responde, PULA (sai 0), como antes.
set -eu
cd "$(dirname "$0")/.."
. tests/servidor_local.sh

work=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-rec-e2e.XXXXXX)
DEV=""
if [ $# -ge 1 ]; then
  BASE="$1"
else
  BASE=""
  NV_W="$(nv_wrangler_bin)" || { echo "FALHOU: wrangler 4.x nao instalado (NUVIO_WRANGLER)"; exit 1; }
  nv_isolate_env "$work"
  nv_rec_test_config "$work/wrangler.toml"
  DEVINFO="$(nv_dev_start "$work/wrangler.toml" "$work")" || exit 1
  PORT=${DEVINFO%% *}; DEV=${DEVINFO##* }
  BASE="http://127.0.0.1:$PORT"
fi

NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-rec-e2e.XXXXXX)
export NUVIO_DADOS
trap 'nv_dev_stop "$DEV"; rm -rf "$NUVIO_DADOS" "$work"' EXIT

if ! curl -s -m 3 "$BASE/v1/saude" | grep -q '"ok"'; then
  if [ -n "$DEV" ]; then echo "FALHOU: servidor local nao subiu"; exit 1; fi
  echo "recomenda_e2e: servidor local fora do ar; PULADO"
  exit 0
fi

# Esquema + migracoes + sessoes base. No modo proprio: mesmo --persist-to do
# dev server; no modo externo: o .wrangler/state padrao do projeto, como antes.
D1ENV="env NUVIO_WRANGLER=${NV_W:-} NUVIO_D1_LOG=$work/d1.log"
if [ -n "$DEV" ]; then
  D1ENV="env NUVIO_WRANGLER=${NV_W:-} NUVIO_D1_LOG=$work/d1.log NUVIO_D1_PERSIST=$work/persist NUVIO_D1_CFG=$work/wrangler.toml"
fi
$D1ENV bash servidor/recomendacoes/preparar-local.sh > "$work/prep.log" 2>&1 \
  || { tail -20 "$work/prep.log"; exit 1; }
d1_seed() {
  if [ -n "$DEV" ]; then
    nv_d1_exec "$work/wrangler.toml" "$work/persist" --command "$1" >/dev/null 2>&1
  else
    "$NV_W" d1 execute nuvio-recomendacoes --local \
      --config servidor/recomendacoes/wrangler.toml --command "$1" >/dev/null 2>&1
  fi
}

# A MESMA CURTO-CIRCUITACAO de servidor/recomendacoes/teste.sh: a verificacao de
# identidade e o unico ponto do servico que fala com a internet (Trakt ou
# Supabase). Uma linha em `sessao` com o SHA-256 de "<via>:<token>" e o que o
# cache do servidor gravaria depois da primeira verificacao. O TOKEN PRECISA
# TER FORMA DE JWT (a.b.c com sub): sessao_iniciar() do cliente descarta
# arquivo com sessao pela metade (#223) e um token de uma palavra so morre
# antes da rede — o header .e2e nem e JSON de proposito, entao o Worker fica
# so com a linha do cache e nunca tenta JWKS/Supabase.
TOK_A="e2e.eyJzdWIiOiJlMmUiLCJleHAiOjk5OTk5OTk5OTl9.e2e"
TOK_B="e2e.eyJzdWIiOiJlMmUtYiIsImV4cCI6OTk5OTk5OTk5OX0.e2e"
HASH=$(printf "nuvio:$TOK_A" | shasum -a 256 | cut -d' ' -f1)
d1_seed "INSERT OR REPLACE INTO sessao (hash,id,nome,expira) \
    VALUES ('$HASH','nuvio:e2e','Teste E2E',$(( $(date +%s) + 86400 )))"

# UMA RECOMENDACAO DE VERDADE ESPERANDO NO SERVIDOR. Sem ela o teste provaria
# so que o 200 vazio e o 304 funcionam — e o parse, que e o que mais quebra,
# nunca teria sido exercitado contra o JSON que o Worker emite de fato.
HASH_B=$(printf "nuvio:$TOK_B" | shasum -a 256 | cut -d' ' -f1)
d1_seed "INSERT OR REPLACE INTO sessao (hash,id,nome,expira) \
    VALUES ('$HASH_B','nuvio:e2e-b','Amigo E2E',$(( $(date +%s) + 86400 )))"
A=(-H "authorization: Bearer $TOK_A" -H "x-nuvio-auth: nuvio" -H "content-type: application/json")
B=(-H "authorization: Bearer $TOK_B" -H "x-nuvio-auth: nuvio" -H "content-type: application/json")
COD=$(curl -s -X POST "${A[@]}" "$BASE/v1/eu" | sed -E 's/.*"codigo":"([a-z0-9]{6})".*/\1/')
curl -s -X POST "${B[@]}" "$BASE/v1/eu" > /dev/null
curl -s -X POST "${B[@]}" -d "{\"codigo\":\"$COD\"}" "$BASE/v1/contatos" > /dev/null
# O D1 E NOVO A CADA RODADA, mas o servidor so aceita 5 envios por par por dia.
# No modo externo o estado era o .wrangler do projeto e a SEXTA execucao no
# mesmo dia recebia 429 aqui — falha medida em 16/09/2026 sem nada de errado
# no cliente. A limpeza continua de proposito.
d1_seed "DELETE FROM rec WHERE para = 'nuvio:e2e' OR para = 'nuvio:e2e-b'"

# `nota` VAI NO CORPO porque e assim que o cliente a manda: quem recomenda tem
# o CatItem na mao e e o unico que tem. O valor e em centesimos (88 = 8,8).
curl -s -X POST "${B[@]}" -d '{"para":"nuvio:e2e","imdb":"tt0111161","tipo":"movie","titulo":"Um Sonho de Liberdade","poster":"https://exemplo/p.jpg","ano":"1994","modelo":2,"nota":88}' \
  "$BASE/v1/rec" > /dev/null

# A COMUNIDADE: os dois publicam o perfil (a lista e reciproca) e o amigo liga
# "vistos recentemente" com um titulo, que tem de chegar como `vendo`.
curl -s -X POST "${A[@]}" -d '{"apelido":"Teste E2E"}' "$BASE/v1/perfil" > /dev/null
curl -s -X POST "${B[@]}" -d '{"apelido":"Amigo E2E","recentes":1}' "$BASE/v1/perfil" > /dev/null
curl -s -X POST "${B[@]}" -d '{"imdb":"tt0111161","titulo":"Um Sonho de Liberdade","agora":0}' "$BASE/v1/atividade" > /dev/null

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/recomenda.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/recomenda_e2e.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-recomenda-e2e \
  -DNV_REC_URL="\"$BASE\"" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-recomenda-e2e
