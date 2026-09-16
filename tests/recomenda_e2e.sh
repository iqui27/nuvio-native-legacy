#!/bin/bash
# O cliente C contra o Worker de verdade, em LOCAL. Ver o cabecalho de
# tests/recomenda_e2e.c.
#
#   cd servidor/recomendacoes && npx wrangler@4 dev --local --port 8799 &
#   bash tests/recomenda_e2e.sh
#
# Sem servidor no ar ele PULA (sai 0): a suite nao pode depender de um servico
# que so existe na maquina de quem o subiu.
set -eu
cd "$(dirname "$0")/.."
BASE="${1:-http://127.0.0.1:8799}"

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-rec-e2e.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

if ! curl -s -m 3 "$BASE/v1/saude" | grep -q '"ok"'; then
  echo "recomenda_e2e: servidor local fora do ar; PULADO"
  exit 0
fi

# A MESMA CURTO-CIRCUITACAO de servidor/recomendacoes/teste.sh: a verificacao de
# identidade e o unico ponto do servico que fala com a internet (Trakt ou
# Supabase). Uma linha em `sessao` com o SHA-256 de "<via>:<token>" e o que o
# cache do servidor gravaria depois da primeira verificacao.
HASH=$(printf 'nuvio:tok-e2e' | shasum -a 256 | cut -d' ' -f1)
( cd servidor/recomendacoes && npx wrangler@4 d1 execute nuvio-recomendacoes \
    --local --command "INSERT OR REPLACE INTO sessao (hash,id,nome,expira) \
      VALUES ('$HASH','nuvio:e2e','Teste E2E',$(( $(date +%s) + 86400 )))" \
    >/dev/null 2>&1 )

# UMA RECOMENDACAO DE VERDADE ESPERANDO NO SERVIDOR. Sem ela o teste provaria
# so que o 200 vazio e o 304 funcionam — e o parse, que e o que mais quebra,
# nunca teria sido exercitado contra o JSON que o Worker emite de fato.
HASH_B=$(printf 'nuvio:tok-e2e-b' | shasum -a 256 | cut -d' ' -f1)
( cd servidor/recomendacoes && npx wrangler@4 d1 execute nuvio-recomendacoes \
    --local --command "INSERT OR REPLACE INTO sessao (hash,id,nome,expira) \
      VALUES ('$HASH_B','nuvio:e2e-b','Amigo E2E',$(( $(date +%s) + 86400 )))" \
    >/dev/null 2>&1 )
A=(-H "authorization: Bearer tok-e2e"   -H "x-nuvio-auth: nuvio" -H "content-type: application/json")
B=(-H "authorization: Bearer tok-e2e-b" -H "x-nuvio-auth: nuvio" -H "content-type: application/json")
COD=$(curl -s -X POST "${A[@]}" "$BASE/v1/eu" | sed -E 's/.*"codigo":"([a-z0-9]{6})".*/\1/')
curl -s -X POST "${B[@]}" "$BASE/v1/eu" > /dev/null
curl -s -X POST "${B[@]}" -d "{\"codigo\":\"$COD\"}" "$BASE/v1/contatos" > /dev/null
# O D1 LOCAL E PERSISTENTE, E O SERVIDOR SO ACEITA 5 ENVIOS POR PAR POR DIA.
# Sem esta limpeza, a SEXTA execucao do teste no mesmo dia recebe 429 na linha
# abaixo, nenhuma recomendacao nova e plantada, e o teste falha em "ela conta
# para o selo: 0" — uma falha que nao tem nada a ver com o cliente e que so
# aparece para quem ja rodou a suite cinco vezes. MEDIDO em 16/09/2026.
( cd servidor/recomendacoes && npx wrangler@4 d1 execute nuvio-recomendacoes \
    --local --command "DELETE FROM rec WHERE para = 'nuvio:e2e' OR para = 'nuvio:e2e-b'" \
    >/dev/null 2>&1 )

# `nota` VAI NO CORPO porque e assim que o cliente a manda: quem recomenda tem
# o CatItem na mao e e o unico que tem. O valor e em centesimos (88 = 8,8).
curl -s -X POST "${B[@]}" -d '{"para":"nuvio:e2e","imdb":"tt0111161","tipo":"movie","titulo":"Um Sonho de Liberdade","poster":"https://exemplo/p.jpg","ano":"1994","modelo":2,"nota":88}' \
  "$BASE/v1/rec" > /dev/null

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/recomenda.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/recomenda_e2e.c -Isrc -o /tmp/nuvio-recomenda-e2e \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-recomenda-e2e
