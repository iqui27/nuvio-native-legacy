#!/bin/bash
# Publica o nuvio-juntos (salas do Watch Together, servidor/juntos) e confere no ar.
#
# SO ESTE SCRIPT PUBLICA O nuvio-juntos, e so de dentro desta arvore: ele
# confere que o wrangler.toml usado e o de servidor/juntos (nome, Durable
# Object, sem [assets]) antes de qualquer deploy — a mesma licao do
# vidaa-publicar.sh e da queda da VIDAA de 06/10.
#
# Primeira vez (ou troca de segredo): o MESMO valor nos dois Workers.
#   tools/juntos-publicar.sh --segredo     # pede o valor, grava nos dois
# Depois:
#   tools/juntos-publicar.sh               # testa, publica e confere /saude
#
# ORDEM no primeiro lancamento: migracao-010 no D1 -> segredo nos dois ->
# este script -> deploy do nuvio-recomendacoes A PARTIR DE master.
set -euo pipefail
cd "$(dirname "$0")/.."

DIR=servidor/juntos
URL="${NUVIO_JUNTOS_URL:-https://nuvio-juntos.henriquef29.workers.dev}"
W="$DIR/node_modules/.bin/wrangler"

grep -q '^name = "nuvio-juntos"$' "$DIR/wrangler.toml" || { echo "juntos-publicar.sh: $DIR/wrangler.toml nao e o do nuvio-juntos" >&2; exit 1; }
grep -q 'class_name = "Sala"' "$DIR/wrangler.toml" || { echo "juntos-publicar.sh: falta o Durable Object Sala" >&2; exit 1; }
if grep -qE '^\[assets\]|^\[\[services\]\]' "$DIR/wrangler.toml"; then
  echo "juntos-publicar.sh: [assets] ou [[services]] no nuvio-juntos — recusado" >&2; exit 1
fi
[ -x "$W" ] || (cd "$DIR" && npm install --no-audit --no-fund)

if [ "${1:-}" = "--segredo" ]; then
  read -r -s -p "SALA_SEGREDO (o mesmo nos dois Workers): " S; echo
  [ ${#S} -ge 32 ] || { echo "juntos-publicar.sh: use 32+ caracteres (ex.: openssl rand -hex 32)" >&2; exit 1; }
  printf '%s' "$S" | "$W" secret put SALA_SEGREDO --config "$DIR/wrangler.toml"
  printf '%s' "$S" | "$W" secret put SALA_SEGREDO --config servidor/recomendacoes/wrangler.toml
  echo "segredo gravado nos dois; agora rode sem --segredo para publicar."
  exit 0
fi

(cd "$DIR" && node --test teste-logica.mjs && node --test teste-juntos.mjs)
"$W" deploy --config "$DIR/wrangler.toml"

curl -fsS "$URL/saude" | grep -q '"ok":1' || { echo "juntos-publicar.sh: ERRO — $URL/saude nao responde" >&2; exit 1; }
echo "juntos-publicar.sh: nuvio-juntos no ar em $URL"
