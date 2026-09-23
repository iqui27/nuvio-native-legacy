#!/bin/bash
# Rota /v1/xtream do servico de recomendacoes (#112): allowlist, SSRF, imagem,
# teto e log sem credencial. Node puro, sem wrangler nem rede — ver
# servidor/recomendacoes/teste-xtream.mjs. Sem node na maquina, pula.
set -eu
cd "$(dirname "$0")/.."
command -v node >/dev/null || { echo "sem node: pulado"; exit 0; }
node --no-warnings servidor/recomendacoes/teste-xtream.mjs
