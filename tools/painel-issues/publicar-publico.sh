#!/bin/bash
# Publica o roadmap publico (gerar.py --publico) no Worker de assets
# nuvio-legacy-roadmap. So publica se tests/publico.sh passar: o build
# normal tem notas internas e nunca sai daqui.
# Uso: publicar-publico.sh [raiz-do-repo]
set -euo pipefail
raiz="${1:-$(git rev-parse --show-toplevel)}"
nome=nuvio-legacy-roadmap
url=https://$nome.henriquef29.workers.dev

sh "$raiz/tools/painel-issues/tests/publico.sh" >/dev/null || { echo "publico.sh falhou: nada publicado"; exit 1; }

out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
python3 "$raiz/tools/painel-issues/gerar.py" --publico --repo "$raiz" --out "$out/site"
# Diretorio temporario: sem wrangler.toml na arvore (o de servidor/recomendacoes e outro worker).
cd "$out"
npx --yes wrangler@4.149.0 deploy --name "$nome" --assets site --compatibility-date 2026-10-01 2>&1 \
  | grep -E "Uploaded $nome|ERROR" || true

curl -fsS --max-time 15 "$url/data.json" \
  | python3 -c "import json,sys; d=json.load(sys.stdin); assert d['meta']['publico'] is True; print('publico no ar:', '$url')"
